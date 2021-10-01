#include "common.h"
#include "dcf77.h"
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

ISR (TIMER1_CAPT_vect)
{
   dcf77_flags.counter = ICR1;
   dcf77_flags.trigger = 1;

   if (dcf77_flags.next_edge)
      TCCR1B &= ~_BV (ICES1);
   else
      TCCR1B |= _BV (ICES1);
   TIFR1 |= _BV (ICF1);
   dcf77_flags.next_edge ^= 1;
}

void dcf77_setup ()
{
   DCF77_POWER_DDR |= _BV (DCF77_POWER);
   dcf77_power_set (1);

   TCCR1B |= _BV (ICNC1); // ICES1 reset -> trigger on falling edge
   dcf77_flags.next_edge = 0;
   TIMSK1 |= _BV (ICIE1); // enable capture interrupt on ICP1 data pin
}

void dcf77_power_set (char on)
{
   if (on)
      DCF77_POWER_PORT |= _BV (DCF77_POWER);
   else
      DCF77_POWER_PORT &= ~_BV (DCF77_POWER);
}
