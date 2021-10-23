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
enum dcf77_state_t state = POWEROFF;
uint16_t state_seconds = 0; // seconds since state transition

uint8_t phase_acc [F_TICK]; // cumulative no. of active level observations

// The '1' bit is 10 samples long. We start sampling slightly early
// (by about 1-2 jiffies) due to the way the phase detector syncs,
// and leave a similar amount of slack at the end.
#define N_BIT_SAMPLES 15
uint8_t bit_samples [N_BIT_SAMPLES];

uint8_t recv_bit_cnt = 0;
int8_t recv_bits [60];

// the time decoded from DCF77, to be loaded into/compared with local clock
struct clock_t dcf77_clock;

uint8_t has_been_synced = 0;
uint32_t seconds_since_last_sync;
uint8_t acc_drift_phase = 0;
uint32_t sync_ttl = 3600; // seconds before a new sync will be attempted

void dcf77_setup ()
{
   DCF77_POWER_DDR |= _BV (DCF77_POWER);

   dcf77_power_set (1);
   state = WARMUP;
   state_seconds = 0;
}

void dcf77_power_set (char on)
{
   if (on)
   {
      DCF77_POWER_PORT |= _BV (DCF77_POWER);
      // switch DATA pin to input with pull-up:
      DCF77_DATA_DDR &= ~_BV (DCF77_DATA);
      DCF77_DATA_PORT |= _BV (DCF77_DATA);
   }
   else
   {
      DCF77_POWER_PORT &= ~_BV (DCF77_POWER);
      // drive DATA low to avoid bleeding current through pull-up:
      DCF77_DATA_DDR |= _BV (DCF77_DATA);
      DCF77_DATA_PORT &= ~_BV (DCF77_DATA);
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
   if (state == POWEROFF)
      return;

   dcf77_flags.curr_level = DCF77_DATA_PIN & _BV (PB0);
   lcd_set_dot5_immediate (dcf77_flags.curr_level);

   switch (state)
   {
   case PHASE_ACQ:
      // we use the inverted signal
      if (! dcf77_flags.curr_level)
      {
         uint8_t j, k;
         for (j = 0, k = clock.jiffies; j < 5; ++j)
         {
            ++ phase_acc [k];
            if (++k == F_TICK)
               k = 0;
         }
      }
      break;

   case FRAME_ACQ:
      if (clock.jiffies < N_BIT_SAMPLES)
      {
         // we use the inverted signal
         bit_samples [clock.jiffies] = ! dcf77_flags.curr_level;
      }
      else if (clock.jiffies == N_BIT_SAMPLES)
      {
         uint8_t k, sum = 0;
         for (k = 0; k < N_BIT_SAMPLES; ++k)
            sum += bit_samples [k];

         if (++ recv_bit_cnt == 60)
            recv_bit_cnt = 0;

         if (sum > 7)
            recv_bits [recv_bit_cnt] = 1;
         else if (sum > 2)
            recv_bits [recv_bit_cnt] = 0;
         else
         {
            recv_bits [recv_bit_cnt] = -1;

            if (recv_bit_cnt != 59)
            {
               // rotate bits so this is bit 59
               int8_t tmp [60];
               for (k = 0; k < 60; ++k)
                  tmp [k] = recv_bits [(k + recv_bit_cnt + 1) % 60];
               for (k = 0; k < 60; ++k)
                  recv_bits [k] = tmp [k];
            }

            for (k = 0; k < 60; ++k)
            {
               if (k % 10 == 0)
                  putchar (' ');

               switch (recv_bits [k])
               {
               case 0: putchar ('0'); break;
               case 1: putchar ('1'); break;
               case -1: putchar ('_'); break;
               default: putchar ('*'); break;
               }
            }
            put_str ("\r\n");

            if (state_seconds >= 60)
            {
               // attempt to decode the time -- if it's valid, we're done!

               uint8_t parity = 0;
               uint8_t minutes = 0;
               bcd_decode (recv_bits + 21, 7, &parity, &minutes);
               if ((parity + recv_bits [28]) & 1)
               {
                  put_str ("perr minutes\r\n");
                  return;
               }

               parity = 0;
               uint8_t hours = 0;
               bcd_decode (recv_bits + 29, 6, &parity, &hours);
               if ((parity + recv_bits [35]) & 1)
               {
                  put_str ("perr hours\r\n");
                  return;
               }

               parity = 0;
               uint8_t day_of_month = 0;
               uint8_t day_of_week = 0;
               uint8_t month = 0;
               uint8_t year = 0;
               bcd_decode (recv_bits + 36, 6, &parity, &day_of_month);
               bcd_decode (recv_bits + 42, 3, &parity, &day_of_week);
               bcd_decode (recv_bits + 45, 5, &parity, &month);
               bcd_decode (recv_bits + 50, 8, &parity, &year);
               if ((parity + recv_bits [58]) & 1)
               {
                  put_str ("perr date\r\n");
                  return;
               }

               for (k = 0; k < 59; ++k)
               {
                  if (recv_bits [k] == -1)
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

               state = SYNCED;
               state_seconds = 0;
               dcf77_clock.hours = hours;
               dcf77_clock.minutes = minutes;
               put_str (" -> SYNCED\r\n");
            }
         }
      }
      break;

   default:
      break;
   }
}

void dcf77_on_second ()
{
   if (has_been_synced)
      ++ seconds_since_last_sync;

   switch (state)
   {
   case POWEROFF:
      if (sync_ttl == 0)
      {
         dcf77_power_set (1);
         state = WARMUP;
         state_seconds = 0;
         put_str (" -> WARMUP\r\n");
      }
      else
         -- sync_ttl;
      break;

   case WARMUP:
      if (++ state_seconds == 8)
      {
         state = PHASE_ACQ;
         state_seconds = 0;
         put_str (" -> PHASE_ACQ\r\n");

         uint8_t j;
         for (j = 0; j < F_TICK; ++j)
            phase_acc [j] = 0;
      }
      break;

   case PHASE_ACQ:
      if ((++state_seconds % 8) == 0)
      {
         uint8_t j;
         uint8_t max_j = 0;    // index (jiffy) of phase_acc sum peak
         uint8_t max_pa = 0;   // phase_acc peak value
         uint8_t max_pa_2 = 0; // phase_acc 2nd largest peak value
         for (j = 0; j < F_TICK; ++j)
         {
            if (phase_acc [j] > max_pa)
            {
               max_j = j;
               max_pa = phase_acc [j];
            }
            if (phase_acc [j] > 0)
               putchar ('A' + phase_acc [j] - 1);
            else
               putchar ('_');
         }
         put_str ("\r\n");
         for (j = 0; j < F_TICK; ++j)
         {
            if (j != max_j && j != max_j + 1 && phase_acc [j] > max_pa_2)
               max_pa_2 = phase_acc [j];
         }

         // The condition of successful detection is a reasonably high
         // singular peak one or two jiffies long; all other readings
         // must be lower.
         uint8_t thresh = 4 * state_seconds;
         if (max_pa > thresh && max_pa > max_pa_2)
         {
            // Offset the detected phase so we sync to jiffy 5 instead of
            // jiffy 0. By doing a rolling sum of length 5 instead of a
            // proper convolution on the estimated signal, we slightly
            // overshoot max_j (by about 4 jiffies), plus leave some slack
            // for the LCD update (another jiffy) so visible seconds ticks
            // will be in perfect sync with a reference clock.
            max_j += F_TICK - 5;
            while (max_j >= F_TICK)
               max_j -= F_TICK;

            put_str ("drift=");
            put_uint (max_j);
            put_str ("\r\n");

            clock_drift_phase (max_j);
            acc_drift_phase += max_j;

            state = FRAME_ACQ;
            state_seconds = 0;
            recv_bit_cnt = 0;

            put_str (" -> FRAME_ACQ\r\n");
         }
         else if (state_seconds == 48) // more than this would overflow phase_acc
         {
            // give up, try again later
            dcf77_power_set (0);
            state = POWEROFF;
            state_seconds = 0;
            sync_ttl = 3600;
            put_str (" -> POWEROFF\r\n");
         }
      }
      break;

   case FRAME_ACQ:
      if (++state_seconds > 330)
      {
         // give up, try again later
         dcf77_power_set (0);
         state = POWEROFF;
         state_seconds = 0;
         sync_ttl = 3600;
         put_str (" -> POWEROFF\r\n");
      }
      break;

   case SYNCED:
   {
      int32_t ppm_adj = 0;
      if (has_been_synced)
      {
         int16_t dt = // numeric range covers for >~10 minutes of discrepancy
            ((dcf77_clock.hours - clock.hours) * 60 +
             (dcf77_clock.minutes - clock.minutes)) * 60 - clock.seconds;
         dt = F_TICK * dt + acc_drift_phase;

         ppm_adj = -2048 * 1000000 / seconds_since_last_sync * dt / F_TICK;

         put_str ("adp=");
         put_uint (acc_drift_phase);
         put_str (" dt=");
         put_int (dt);
         put_str (" ppm_adj=");
         put_q4_11 (ppm_adj);
         put_str ("\r\n");
      }

      clock_set_hm (dcf77_clock.hours, dcf77_clock.minutes);
      seconds_since_last_sync = 0;
      has_been_synced = 1;
      acc_drift_phase = 0;

      dcf77_power_set (0);
      state = POWEROFF;
      state_seconds = 0;
      sync_ttl = 86400 - 120; // It takes ~2 mins to get a sync
      put_str (" -> POWEROFF\r\n");

      if (ppm_adj)
         clock_adjust (ppm_adj);
   }
      break;
   }
}
