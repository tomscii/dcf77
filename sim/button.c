/* Taken verbatim from simavr (+formatting), because the installed
 * version does not support button_release ().
 */

#include <stdlib.h>
#include <stdio.h>

#include <simavr/sim_avr.h>

#include "button.h"

static avr_cycle_count_t
button_auto_release (avr_t * avr, avr_cycle_count_t when, void * param)
{
   button_t * b = (button_t *)param;
   avr_raise_irq(b->irq + IRQ_BUTTON_OUT, 1);
   printf("button_auto_release\n");
   return 0;
}

/*
 * button release. set the "pin" to one
 */
void
button_release (button_t* b)
{
   avr_cycle_timer_cancel (b->avr, button_auto_release, b);
   avr_raise_irq (b->irq + IRQ_BUTTON_OUT, 1); // release
}

/*
 * button press. set the "pin" to zero and optionally register a timer
 * that will reset it in a few usecs
 */
void
button_press (button_t* b, uint32_t duration_usec)
{
   avr_cycle_timer_cancel (b->avr, button_auto_release, b);
   avr_raise_irq (b->irq + IRQ_BUTTON_OUT, 0); // press
   if (duration_usec) {
      // register the auto-release
      avr_cycle_timer_register_usec (b->avr, duration_usec,
                                     button_auto_release, b);
   }
}

void
button_init (avr_t* avr, button_t* b, const char* name)
{
   b->irq = avr_alloc_irq (&avr->irq_pool, 0, IRQ_BUTTON_COUNT, &name);
   b->avr = avr;
}
