#include "common.h"
#include "clock.h"

#include <inttypes.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>

#define F_TICK 50
#define TICK_DIV (F_CPU/F_TICK - 1)

struct clock_t clock;
volatile struct clock_flags_t clock_flags;

ISR (TIMER1_OVF_vect)
{
   if (clock_flags.skip_jiffy)
      clock_flags.skip_jiffy = 0;
   else
   {
      clock_plus_jiffy ();
      clock_flags.tick = 1;
   }
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
   clock_flags.skip_jiffy = 1;
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
}

void
clock_set (uint8_t hours, uint8_t minutes, uint8_t seconds)
{
   clock.hours = hours;
   clock.minutes = minutes;
   clock.seconds = seconds;
   clock.jiffies = 0;
   TCNT1 = 0;
}
