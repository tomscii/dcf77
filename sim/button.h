/* Taken verbatim from simavr (+formatting), because the installed
 * version does not support button_release ().
 */

#pragma once

#include "sim_irq.h"

enum {
   IRQ_BUTTON_OUT = 0,
   IRQ_BUTTON_COUNT
};

typedef struct button_t {
   avr_irq_t * irq; // output irq
   struct avr_t * avr;
   uint8_t value;
} button_t;

void button_init(struct avr_t * avr, button_t * b, const char * name);

void button_press(button_t * b, uint32_t duration_usec);

void button_release(button_t * b);
