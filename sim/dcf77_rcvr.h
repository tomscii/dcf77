#pragma once

#include "sim_irq.h"

enum
{
   IRQ_DCF77_POWER = 0,
   IRQ_DCF77_DATA,
   IRQ_DCF77_COUNT
};

typedef struct dcf77_rcvr_t
{
   avr_irq_t* irq;
   struct avr_t* avr;
   uint8_t power: 1;
   uint8_t data: 1;
} dcf77_rcvr_t;

void dcf77_rcvr_init (struct avr_t* avr, dcf77_rcvr_t* p);
