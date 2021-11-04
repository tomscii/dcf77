#include "common.h"
#include "pbtn.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#define PBTN_PORT       PORTB
#define PBTN_DDR        DDRB
#define PBTN_PIN        PINB
#define PBTN_MODE       PB4
#define PBTN_SET        PB2
#define PBTN_ADJ        PB1

volatile struct pbtn_flags_t pbtn_flags;

static int tick_count = 0;

ISR (PCINT0_vect)
{
   uint8_t pval = PBTN_PIN;
   pbtn_flags.momentary = (pval & _BV (PBTN_MODE)) == 0;
   pbtn_flags.momentary |= ((pval & _BV (PBTN_SET)) == 0) << 1;
   pbtn_flags.momentary |= ((pval & _BV (PBTN_ADJ)) == 0) << 2;
}

void
pbtn_setup ()
{
   // Set up button pins as inputs with pull-up
   PBTN_PORT |= _BV (PBTN_MODE) | _BV (PBTN_SET) | _BV (PBTN_ADJ);
   PBTN_DDR &= ~(_BV (PBTN_MODE) | _BV (PBTN_SET) | _BV (PBTN_ADJ));

   // Enable pin change interrupt on these pins
   PCMSK0 |= _BV (PBTN_MODE) | _BV (PBTN_SET) | _BV (PBTN_ADJ);
   PCICR |= _BV (PCIE0);
}

void
pbtn_on_tick ()
{
   if (pbtn_flags.momentary)
   {
      if (tick_count < 250)
         ++ tick_count;

      pbtn_flags.pending_event |= pbtn_flags.momentary;
      if (tick_count == CNT_THRESH_VERYLONG &&
          !(pbtn_flags.pending_event & F_EVT_VERYLONG))
      {
         pbtn_flags.pending_event |= F_EVT_VERYLONG;
         pbtn_flags.ack_event = 0;
      }
      else if (tick_count == CNT_THRESH_LONG)
      {
         pbtn_flags.pending_event |= F_EVT_LONG;
         pbtn_flags.ack_event = 0;
      }
   }
   else if (pbtn_flags.previous)
   {
      // no button is pressed, generate release event
      if (tick_count < CNT_THRESH_LONG)
      {
         pbtn_flags.pending_event = F_EVT_REGULAR | pbtn_flags.previous;
         pbtn_flags.ack_event = 0;
      }
   }
   else
   {
      tick_count = 0;

      if (pbtn_flags.ack_event)
      {
         pbtn_flags.ack_event = 0;
         pbtn_flags.pending_event = 0;
      }
   }

   pbtn_flags.previous = pbtn_flags.momentary;
}
