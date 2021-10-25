#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <simavr/sim_avr.h>

#include "lcd_display.h"

static void lcd_d0_hook(struct avr_irq_t* irq, uint32_t value, void* param)
{
   lcd_display_t* p = (lcd_display_t *)param;
   p->i0 = value;
}

static void lcd_d1_hook(struct avr_irq_t* irq, uint32_t value, void* param)
{
   lcd_display_t* p = (lcd_display_t *)param;
   p->i1 = value;
}

static void lcd_d2_hook(struct avr_irq_t* irq, uint32_t value, void* param)
{
   lcd_display_t* p = (lcd_display_t *)param;
   p->i2 = value;
}

static void lcd_clk_hook(struct avr_irq_t* irq, uint32_t value, void* param)
{
   lcd_display_t* p = (lcd_display_t *)param;
   if (!irq->value && value) { // rising edge
      p->s0 = (p->s0 << 1) | p->d0;
      p->s1 = (p->s1 << 1) | p->d1;
      p->s2 = (p->s2 << 1) | p->d2;
      p->d0 = p->i0;
      p->d1 = p->i1;
      p->d2 = p->i2;
   }
}

static void lcd_com_hook(struct avr_irq_t* irq, uint32_t value, void* param)
{
   lcd_display_t* p = (lcd_display_t *)param;
   p->com = value;
}

static void lcd_oe__hook(struct avr_irq_t* irq, uint32_t value, void* param)
{
   lcd_display_t* p = (lcd_display_t *)param;
   if (irq->value && !value) { // falling edge
      uint16_t v0 = p->s0;
      uint16_t v1 = p->s1;
      uint16_t v2 = p->s2;
      if (p->com) // invert
      {
         v0 = ~v0;
         v1 = ~v1;
         v2 = ~v2;
      }
      if (v0 != p->v0 || v1 != p->v1 || v2 != p->v2)
      {
         p->v0 = v0;
         p->v1 = v1;
         p->v2 = v2;

#define FMT_BINARY "%c%c%c%c%c%c%c%c"
#define BINARY(b)                               \
            (b & 0x80 ? '1' : '0'),             \
            (b & 0x40 ? '1' : '0'),             \
            (b & 0x20 ? '1' : '0'),             \
            (b & 0x10 ? '1' : '0'),             \
            (b & 0x08 ? '1' : '0'),             \
            (b & 0x04 ? '1' : '0'),             \
            (b & 0x02 ? '1' : '0'),             \
            (b & 0x01 ? '1' : '0')

         fprintf (p->gui_in,
                  "lcd: " FMT_BINARY FMT_BINARY FMT_BINARY FMT_BINARY FMT_BINARY FMT_BINARY "\n",
                  BINARY (v0 >> 8), BINARY (v0 & 0xff),
                  BINARY (v1 >> 8), BINARY (v1 & 0xff),
                  BINARY (v2 >> 8), BINARY (v2 & 0xff));
         fflush (p->gui_in);

#undef BINARY
#undef FMT_BINARY
      }
   }
   p->oe_ = value;
}

static const char* irq_names[IRQ_LCD_COUNT] = {
   [IRQ_LCD_D0] = "lcd.D0",
   [IRQ_LCD_D1] = "lcd.D1",
   [IRQ_LCD_D2] = "lcd.D2",
   [IRQ_LCD_CLK] = "lcd.CLK",
   [IRQ_LCD_COM] = "lcd.COM",
   [IRQ_LCD_OE_] = "lcd.OE_",
};

void
lcd_display_init(struct avr_t* avr, lcd_display_t* p, FILE* gui_in)
{
   memset(p, 0, sizeof(*p));

   p->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_LCD_COUNT, irq_names);
   p->gui_in = gui_in;

   avr_irq_register_notify(p->irq + IRQ_LCD_D0, lcd_d0_hook, p);
   avr_irq_register_notify(p->irq + IRQ_LCD_D1, lcd_d1_hook, p);
   avr_irq_register_notify(p->irq + IRQ_LCD_D2, lcd_d2_hook, p);
   avr_irq_register_notify(p->irq + IRQ_LCD_CLK, lcd_clk_hook, p);
   avr_irq_register_notify(p->irq + IRQ_LCD_COM, lcd_com_hook, p);
   avr_irq_register_notify(p->irq + IRQ_LCD_OE_, lcd_oe__hook, p);
}
