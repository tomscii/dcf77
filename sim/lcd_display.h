#pragma once

#include <stdio.h>

#include "sim_irq.h"

enum {
   IRQ_LCD_D0 = 0,
   IRQ_LCD_D1,
   IRQ_LCD_D2,
   IRQ_LCD_CLK,
   IRQ_LCD_COM,
   IRQ_LCD_OE_,
   IRQ_LCD_COUNT
};

typedef struct lcd_display_t {
   avr_irq_t* irq;
   FILE* gui_in;
   uint16_t v0;
   uint16_t v1;
   uint16_t v2;
   uint16_t s0;
   uint16_t s1;
   uint16_t s2;
   uint8_t i0: 1;
   uint8_t i1: 1;
   uint8_t i2: 1;
   uint8_t d0: 1;
   uint8_t d1: 1;
   uint8_t d2: 1;
   uint8_t clk: 1;
   uint8_t com: 1;
   uint8_t oe_: 1;
} lcd_display_t;

void lcd_display_init (struct avr_t* avr, lcd_display_t* p, FILE* gui_in);
