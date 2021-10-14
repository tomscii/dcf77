#include "common.h"
#include "clock.h"
#include "dcf77.h"
#include "lcd.h"
#include "utils.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#define DCF77_POWER_PORT PORTD
#define DCF77_POWER_DDR  DDRD
#define DCF77_POWER      PD7

#define DCF77_DATA_PORT  PORTB
#define DCF77_DATA_DDR   DDRB
#define DCF77_DATA_PIN   PINB
#define DCF77_DATA       PB0

volatile struct dcf77_flags_t dcf77_flags;

enum dcf77_state_t
{
   POWEROFF, WARMUP, PHASE_ACQ, FRAME_ACQ, SYNCED
};
enum dcf77_state_t dcf77_state = POWEROFF;
uint16_t dcf77_state_seconds = 0; // seconds since state transition

// TODO 50 should not be hardwired, it is F_TICK in clock.c
uint8_t dcf77_acc_sig [50]; // cumulative no. of active level observations
uint8_t dcf77_samples [10]; // no need to sample all the way through

uint8_t dcf77_recv_cnt = 0;
int8_t dcf77_recv_bits [60];

struct clock_t dcf77_clock; // the time decoded from DCF77, to be set/compared/...

uint8_t has_been_synced = 0;
uint32_t seconds_since_last_sync;

ISR (TIMER1_CAPT_vect)
{
   dcf77_flags.counter = ICR1;
   dcf77_flags.jiffies = clock.jiffies;
   dcf77_flags.trigger = 1;
   dcf77_flags.curr_level = DCF77_DATA_PIN & _BV (PB0);
   dcf77_flags.next_edge = ~dcf77_flags.curr_level;
   if (dcf77_flags.next_edge)
      TCCR1B |= _BV (ICES1);
   else
      TCCR1B &= ~_BV (ICES1);
   TIFR1 |= _BV (ICF1);

   lcd_set_dot5_immediate (dcf77_flags.curr_level);
}

void dcf77_setup ()
{
   DCF77_POWER_DDR |= _BV (DCF77_POWER);
   dcf77_power_set (1);

   TCCR1B |= _BV (ICNC1); // ICES1 reset -> trigger on falling edge
   dcf77_flags.next_edge = 0;
   dcf77_flags.curr_level = 1;
   TIMSK1 |= _BV (ICIE1); // enable capture interrupt on ICP1 data pin
}

void dcf77_power_set (char on)
{
   if (on)
   {
      DCF77_POWER_PORT |= _BV (DCF77_POWER);
      dcf77_state = WARMUP;
      dcf77_state_seconds = 0;
      put_str (" -> WARMUP\r\n");
   }
   else
   {
      DCF77_POWER_PORT &= ~_BV (DCF77_POWER);
      dcf77_state = POWEROFF;
      dcf77_state_seconds = 0;
      put_str (" -> POWEROFF\r\n");
   }

   lcd_set_dot5_immediate (on);
}

void bcd_decode (int8_t* data, int count, uint8_t* parity, uint8_t* result)
{
   const uint8_t mul [] = {1, 2, 4, 8, 10, 20, 40, 80};

   uint8_t k;
   *result = 0;
   for (k = 0; k < count; ++k)
   {
      *parity += data [k];
      *result += mul [k] * data [k];
   }
}

void dcf77_on_tick ()
{
   if (dcf77_state != POWEROFF)
   {
      // we use the inverted signal
      if (! dcf77_flags.curr_level)
      {
         ++ dcf77_acc_sig [clock.jiffies];
      }
   }

   if (dcf77_state == FRAME_ACQ)
   {
      if (clock.jiffies < 10)
      {
         dcf77_samples [clock.jiffies] = ! dcf77_flags.curr_level;
      }
      else if (clock.jiffies == 10)
      {
         uint8_t k, sum = 0;
         for (k = 0; k < 10; ++k)
            sum += dcf77_samples [k];

         if (++ dcf77_recv_cnt == 60)
            dcf77_recv_cnt = 0;

         if (sum > 7)
            dcf77_recv_bits [dcf77_recv_cnt] = 1;
         else if (sum > 2)
            dcf77_recv_bits [dcf77_recv_cnt] = 0;
         else
         {
            dcf77_recv_bits [dcf77_recv_cnt] = -1;

            if (dcf77_recv_cnt != 59)
            {
               // rotate bits so this is bit 59
               int8_t tmp [60];
               for (k = 0; k < 60; ++k)
                  tmp [k] = dcf77_recv_bits [(k + dcf77_recv_cnt + 1) % 60];
               for (k = 0; k < 60; ++k)
                  dcf77_recv_bits [k] = tmp [k];
            }

            for (k = 0; k < 60; ++k)
            {
               if (k % 10 == 0)
                  putchar (' ');

               switch (dcf77_recv_bits [k])
               {
               case 0: putchar ('0'); break;
               case 1: putchar ('1'); break;
               case -1: putchar ('_'); break;
               default: putchar ('*'); break;
               }
            }
            put_str ("\r\n");

            if (dcf77_state_seconds >= 60)
            {
               // attempt to decode the time
               // if it's valid, we're done!

               uint8_t parity = 0;
               uint8_t minutes = 0;
               bcd_decode (dcf77_recv_bits + 21, 7, &parity, &minutes);
               if ((parity + dcf77_recv_bits [28]) & 1)
               {
                  put_str ("perr minutes\r\n");
                  return;
               }

               parity = 0;
               uint8_t hours = 0;
               bcd_decode (dcf77_recv_bits + 29, 6, &parity, &hours);
               if ((parity + dcf77_recv_bits [35]) & 1)
               {
                  put_str ("perr hours\r\n");
                  return;
               }

               parity = 0;
               uint8_t day_of_month = 0;
               uint8_t day_of_week = 0;
               uint8_t month = 0;
               uint8_t year = 0;
               bcd_decode (dcf77_recv_bits + 36, 6, &parity, &day_of_month);
               bcd_decode (dcf77_recv_bits + 42, 3, &parity, &day_of_week);
               bcd_decode (dcf77_recv_bits + 45, 5, &parity, &month);
               bcd_decode (dcf77_recv_bits + 50, 8, &parity, &year);
               if ((parity + dcf77_recv_bits [58]) & 1)
               {
                  put_str ("perr date\r\n");
                  return;
               }

               for (k = 0; k < 59; ++k)
               {
                  if (dcf77_recv_bits [k] == -1)
                  {
                     put_str ("illegal bit\r\n");
                     return;
                  }
               }
               if (hours > 23 || minutes > 59)
               {
                  put_str ("illegal value\r\n");
                  return;
               }

               dcf77_state = SYNCED;
               dcf77_state_seconds = 0;
               dcf77_clock.hours = hours;
               dcf77_clock.minutes = minutes;
               put_str (" -> SYNCED\r\n");
            }
         }
      }
   }
}

void dcf77_on_second ()
{
   if (has_been_synced)
      ++ seconds_since_last_sync;

   switch (dcf77_state)
   {
   case POWEROFF:
      break;
   case WARMUP:
      if (++ dcf77_state_seconds > 8)
      {
         dcf77_state = PHASE_ACQ;
         dcf77_state_seconds = 0;
         put_str (" -> PHASE_ACQ\r\n");

         uint8_t j;
         for (j = 0; j < 50; ++j)
         {
            dcf77_acc_sig [j] = 0;
         }
      }
      break;
   case PHASE_ACQ:
      if ((++dcf77_state_seconds % 8) == 0)
      {
         uint8_t j, k;
         uint16_t rs [50];     // rolling sums
         uint8_t max_j = 0;    // index (jiffy) of rolling sum peak
         uint8_t max_rs = 0;   // rolling sum peak value
         uint8_t max_rs_2 = 0; // rolling sum 2nd largest peak value
         for (j = 0; j < 50; ++j)
         {
            rs [j] = 0;
            for (k = j; k < j + 5; ++k)
               rs [j] += dcf77_acc_sig [k % 50];
            rs [j] /= dcf77_state_seconds >> 1; // no need to be exact
         }
         for (j = 0; j < 50; ++j)
         {
            if (rs [j] > max_rs)
            {
               max_j = j;
               max_rs = rs [j];
            }
            if (rs [j] > 0)
               putchar ('A' + rs [j] - 1);
            else
               putchar ('_');
         }
         put_str ("\r\n");
         for (j = 0; j < 50; ++j)
         {
            if (j != max_j && j != max_j + 1 && rs [j] > max_rs_2)
               max_rs_2 = rs [j];
         }

         // The condition of successful detection is a reasonably high
         // singular peak one or two jiffies long; all other readings
         // must be lower.
         if (max_rs > 6 && max_rs > max_rs_2)
         {
            dcf77_state = FRAME_ACQ;
            dcf77_state_seconds = 0;
            dcf77_recv_cnt = 0;
            clock_set_phase (max_j, 0);

            uint8_t tmp [50];
            for (j = 0; j < 50; ++j)
               tmp [j] = dcf77_acc_sig [(max_j + j) % 50];
            for (j = 0; j < 50; ++j)
               dcf77_acc_sig [j] = tmp [j];

            put_str (" -> FRAME_ACQ\r\n");
         }
         else if (dcf77_state_seconds == 64)
         {
            // give up, try again later
            dcf77_power_set (0);
         }
      }
      break;
   case FRAME_ACQ:
      if ((++dcf77_state_seconds % 8) == 0)
      {
         if (dcf77_acc_sig [10] == 0 && dcf77_acc_sig [49] > 0 &&
             dcf77_acc_sig [4] < 8)
         {
            // pre-spill only
            clock_set_phase (0, dcf77_acc_sig [49] * 50);
         }
         else if (dcf77_acc_sig [10] > 0 && dcf77_acc_sig [49] == 0 &&
                  dcf77_acc_sig [0] < 8)
         {
            // post-spill only
            clock_set_phase (0, dcf77_acc_sig [10] * -50);
         }

         uint8_t j;
         for (j = 0; j < 50; ++j)
         {
            if (dcf77_acc_sig [j] > 0)
               putchar ('A' + dcf77_acc_sig [j] - 1);
            else
               putchar ('_');

            dcf77_acc_sig [j] >>= 1; // crude exp.avg.
         }
         put_str ("\r\n");
      }
      break;
   case SYNCED:
      if (has_been_synced)
      {
         // TODO calculate local clock deviation
      }
      clock_set_hm (dcf77_clock.hours, dcf77_clock.minutes);
      dcf77_power_set (0);
      has_been_synced = 1;
      seconds_since_last_sync = 0;
      break;
   }
}
