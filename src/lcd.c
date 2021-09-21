#include "common.h"
#include "lcd.h"

#include <avr/io.h>

void
lcd_setup ()
{
   LCD_DDR =
      _BV (LCD_D0) | _BV (LCD_D1) | _BV (LCD_D2) |
      _BV (LCD_CLK) | _BV (LCD_COM) | _BV (LCD_OE_);
   LCD_PORT = _BV (LCD_CLK) | _BV (LCD_OE_);
}

void
lcd_send_data ()
{
   uint16_t seconds = 123;

   /*
     Note: it looks like we will need to make this more efficient,
     since this needs to happen on *every* HZ tick, not just when
     visible LCD content changes (i.e., once per second).
     The reason for this is that the polarity needs to be alternated
     and we have no easy (external) way to do that - only to write
     the binary negated value of all bits on every second tick
     (also negating LCD COM every second tick).

     How about preparing an array of bytes to be pushed out on PORTC?
     Something like:
   */
#if 0
   static uint8_t pca [] = {
      // OE/ COM CLK D2 D1 D0
      // 1   x   0   b2 b1 b0 // data change on clk fall
      // 1   x   1   b2 b1 b0 // data stable on clk rise
      // 1   x   0   b5 b4 b3 // data change on clk fall
      // 1   x   1   b5 b4 b3 // data stable on clk rise
   };
#endif
   /*
     This would allow us to just loop through pca[] and do direct
     writes to PORTC without any bit manipulations. There would be
     2 fetch/write ops per LCD clk, so 32 in total, plus 3 more.
     2 MCU clocks to fetch and 2 to write -> ~70 cycles, ~0.56 ms
     (1 jiffy: 2500 cycles, 20ms)
   */

   // shift the bits of current seconds counter out on DATA0
   uint8_t j;
   PORTC &= ~_BV (PC2); // clk fall (kept high when idle)
#if 0
   // dummy cycle to make timing measurement accurate
   for (j = 0; j < 8; ++j)
   {
      PORTC |= _BV (PC0);
      //PORTC &= ~_BV (PC0);

      //PORTC |= _BV (PC1);
      PORTC &= ~_BV (PC1);

      PORTC |= _BV (PC2); // clk rise
      PORTC &= ~_BV (PC2); // clk fall
   }
#endif

   // real data
   uint16_t sec1 = seconds;
   for (j = 0; j < 16; ++j)
   {
      if (sec1 & 1U)
         PORTC |= _BV (PC0);
      else
         PORTC &= ~_BV (PC0);
      sec1 >>= 1;

#if 0
      if (t0 & 1U)
         PORTC |= _BV (PC1);
      else
         PORTC &= ~_BV (PC1);
#endif

      PORTC |= _BV (PC2); // clk rise
      PORTC &= ~_BV (PC2); // clk fall
   }

   // an extra shift to latch the last bit
   PORTC |= _BV (PC2); // clk rise and keep high for idle state
   //PORTC &= ~_BV (PC2); // clk fall

   // enable shift register output (until Timer1 Compare Match B)
   PORTC &= ~_BV (PC4);
}
