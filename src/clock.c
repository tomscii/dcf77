#include "common.h"
#include "clock.h"
#include "utils.h"

#include <stdint.h>

#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>

#define TICK_CYCLES (F_CPU/F_TICK)
#define TICK_DIV (TICK_CYCLES - 1)
#define CLKCOMP_TICKS (1000000/TICK_CYCLES)

struct clock_t clock;
volatile struct clock_flags_t clock_flags;

uint16_t clkcomp_tick_count = 0;
uint16_t clkcomp_frac_count = 0;
uint16_t clkcomp_frac_count_prev = 0;

/*
  Because the crystal frequency is not precisely accurate, we
  accommodate for small systematic adjustments (in the ppm range).

  We represent the adjustment in Q4.11 16-bit fixed point format,
  stored in field 'ppm', with bits [siiiifff|ffffffff] as below:
    - 1 bit   - sign
    - 4 bits  - integer part
    - 11 bits - fractional part
  Adjustment range +/- 15.9995 ppm, LSB = 1/2048 ppm (~0.0005 ppm)

  We trigger the compensator once per each one-million clock ticks,
  so the integer part can be applied directly (1 ppm = 1 clock to
  compensate). The fractional part is accounted for on the same
  trigger, using a masked counter to produce the correct fractional
  rate of 1-clock compensations. Bits of 'fractional':

    b10: 1 per every 2^1 = 2. tick     -> 1/2 ppm
    b9:  1 per every 2^2 = 4. tick     -> 1/4 ppm
    ...                                   ...
    b0:  1 per every 2^11 = 2048. tick -> 1/2048 ppm ~= 0.0005 ppm

  Because the counter's LSB changes most frequently, the fractional
  part needs to be reversed and aligned so its MSB, b10, becomes b0.
  This reversed value is precomputed and stored in frac_rev.
*/
struct
{
   int16_t ppm;         // Q4.11: [siiiifff|ffffffff]
   int8_t sign;         // 1: speedup, -1: slowdown
   uint16_t integer;    // 4 bits, b3: 8 ppm ... b0: 1 ppm
   uint16_t fractional; // 11 bits, b10: 1/2 ppm ... b0: 1/2048 ppm
   uint16_t frac_rev;   // fractional's b10 -> LSB, etc.
} clock_adj;

uint16_t ee_clock_adj_ppm EEMEM = 0;

ISR (TIMER1_OVF_vect)
{
   if (++clkcomp_tick_count == CLKCOMP_TICKS)
   {
      clkcomp_tick_count = 0;

      clkcomp_frac_count_prev = clkcomp_frac_count;
      clkcomp_frac_count++;

      uint16_t chg = clock_adj.frac_rev;
      chg &= clkcomp_frac_count ^ clkcomp_frac_count_prev;
      chg &= clkcomp_frac_count;
      int16_t comp = clock_adj.sign * (clock_adj.integer + n1bits (chg));
      OCR1A = TICK_DIV - comp;
   }
   else
      OCR1A = TICK_DIV;

   clock_plus_jiffy ();
   clock_flags.tick = 1;
}

void
clock_plus_jiffy ()
{
   if (++clock.jiffies == F_TICK)
   {
      clock.jiffies = 0;
      clock_flags.ovf_jiffies = 1;
      if (++clock.seconds == 60)
      {
         clock.seconds = 0;
         if (++clock.minutes == 60)
         {
            clock.minutes = 0;
            if (++clock.hours == 24)
               clock.hours = 0;
         }
      }
   }
}

void clock_minus_jiffy ()
{
   clock_drift_phase (1);
}

void
clock_setup ()
{
   power_timer1_enable ();

   // Timer 1: Fast PWM, top set by OCR1A, clk /1
   TCCR1A = _BV (WGM11) | _BV (WGM10);
   TCCR1B = _BV (WGM13) | _BV (WGM12) | _BV (CS10);
   OCR1A = TICK_DIV;
   TIMSK1 = _BV (TOIE1);

   uint16_t clock_adj_ppm;
   if ((clock_adj_ppm = eeprom_read_word (&ee_clock_adj_ppm)) != 0xffff)
      clock_adjust ((int16_t) clock_adj_ppm);
}

void
clock_set_hm (uint8_t hours, uint8_t minutes)
{
   cli ();
   clock.hours = hours;
   clock.minutes = minutes;
   clock.seconds = 0;
   sei ();
}

void
clock_set_hms (uint8_t hours, uint8_t minutes, uint8_t seconds)
{
   cli ();
   clock.hours = hours;
   clock.minutes = minutes;
   clock.seconds = seconds;
   clock.jiffies = 0;
   TCNT1 = 0;
   sei ();
}

void
clock_drift_phase (int8_t jiffies)
{
   cli ();
   clock.jiffies -= jiffies;
   sei ();
}

void
clock_adjust (int16_t ppm)
{
   clock_adj.ppm += ppm;
   put_str ("clock_adj set to ");
   put_q4_11 (clock_adj.ppm);
   put_str (" ppm\r\n");

   clock_adj.sign = clock_adj.ppm < 0 ? -1 : 1;
   clock_adj.integer = clock_adj.sign * clock_adj.ppm;
   clock_adj.fractional = clock_adj.integer;
   clock_adj.integer &= 0x7800; // upper 4 bits (no sign bit)
   clock_adj.fractional &= 0x07ff; // lower 11 bits
   clock_adj.integer >>= 11;
   // preprocess so the clockadj cycle has less work to do:
   clock_adj.frac_rev = revbits (clock_adj.fractional << 5);

   eeprom_write_word (&ee_clock_adj_ppm, (uint16_t) clock_adj.ppm);
}

void
clock_adjust_reset ()
{
   clock_adj.ppm = 0;
   clock_adjust (0);
}
