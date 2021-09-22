#include "common.h"
#include "lcd.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#define LCD_PORT PORTC
#define LCD_DDR  DDRC

#define LCD_D0   PC0
#define LCD_D1   PC1
#define LCD_D2   PC2
#define LCD_CLK  PC3
#define LCD_COM  PC4
#define LCD_OE_  PC5

/* Data tables for efficient bit-banging our LCD data
 * into three parallel 16-bit shift registers, sampled
 * on the rising edge of LCD_CLK.
 *
 * Bits according to LCD_PORT pins:
 *    7    6    5    4    3    2    1    0
 *   n/c  n/c  OE/  COM  CLK   D2   D1   D0
 *
 * The OE_ bit is initialized to 1 (not active), enabled separately.
 * The COM bit is initialized to 0, and enabled separately.
 * The CLK bit is initialized to 0 and toggled by code for each cycle.
 *
 * Because we are sending alternating positive and negative frames
 * with inverted data, and doing so with a much higher frequency
 * than the rate of data changes, we keep two arrays, one with the
 * data bits straight-through and one with D2..0 inverted.
 * Both need to be maintained on display content changes.
 */
static uint8_t lcd_data_pos [] = {
   0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27,
   0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27
};
static uint8_t lcd_data_neg [] = {
   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20
};

void lcd_send_frame_pos ();
void lcd_send_frame_neg ();

void
lcd_setup ()
{
   LCD_DDR =
      _BV (LCD_OE_) | _BV (LCD_COM) | _BV (LCD_CLK) |
      _BV (LCD_D2) | _BV (LCD_D1) | _BV (LCD_D0);
   LCD_PORT = _BV (LCD_CLK) | _BV (LCD_OE_);
}

void
lcd_set_bits (uint16_t* data)
{
   uint8_t j = 0;
   for (j = 0; j < 16; ++j)
   {
      uint16_t m = 1 << j;
      uint8_t k = 0;
      for (k = 0; k < 3; ++k)
      {
         uint8_t n = 1 << k;
         if (data [k] & m)
         {
            lcd_data_pos [j] |= n;
            lcd_data_neg [j] &= ~n;
         }
         else
         {
            lcd_data_pos [j] &= ~n;
            lcd_data_neg [j] |= n;
         }
      }
   }
}

// Runtime: 74 MCU clocks
void
lcd_send_frame ()
{
   static uint8_t phase = 0;
   phase ^= 0x01;

   // reset to idle: disable output, LCD_COM=GND, LCD_CLK stays high
   LCD_PORT = _BV (LCD_OE_) | _BV (LCD_CLK);

   if (phase)
      lcd_send_frame_pos ();
   else
      lcd_send_frame_neg ();
}

// unrolled loop for ultimate speed (5 MCU clocks per LCD clock cycle)
#define LCD_BIT_OUT(arr, n)                         \
   v = arr [n];                                     \
   LCD_PORT = v;                                    \
   v |= _BV (LCD_CLK);                              \
   LCD_PORT = v

void
lcd_send_frame_pos ()
{
   uint8_t v;
   LCD_BIT_OUT (lcd_data_pos, 0);
   LCD_BIT_OUT (lcd_data_pos, 1);
   LCD_BIT_OUT (lcd_data_pos, 2);
   LCD_BIT_OUT (lcd_data_pos, 3);
   LCD_BIT_OUT (lcd_data_pos, 4);
   LCD_BIT_OUT (lcd_data_pos, 5);
   LCD_BIT_OUT (lcd_data_pos, 6);
   LCD_BIT_OUT (lcd_data_pos, 7);
   LCD_BIT_OUT (lcd_data_pos, 8);
   LCD_BIT_OUT (lcd_data_pos, 9);
   LCD_BIT_OUT (lcd_data_pos, 10);
   LCD_BIT_OUT (lcd_data_pos, 11);
   LCD_BIT_OUT (lcd_data_pos, 12);
   LCD_BIT_OUT (lcd_data_pos, 13);
   LCD_BIT_OUT (lcd_data_pos, 14);
   LCD_BIT_OUT (lcd_data_pos, 15);

   // extra clock cycle to shift the last bits out + enable output
   LCD_PORT = _BV (LCD_OE_); // LCD_CLK low phase, output enable still off
   LCD_PORT = _BV (LCD_CLK); // LCD_CLK high phase, output enable ON
}
void
lcd_send_frame_neg ()
{
   uint8_t v;
   LCD_BIT_OUT (lcd_data_neg, 0);
   LCD_BIT_OUT (lcd_data_neg, 1);
   LCD_BIT_OUT (lcd_data_neg, 2);
   LCD_BIT_OUT (lcd_data_neg, 3);
   LCD_BIT_OUT (lcd_data_neg, 4);
   LCD_BIT_OUT (lcd_data_neg, 5);
   LCD_BIT_OUT (lcd_data_neg, 6);
   LCD_BIT_OUT (lcd_data_neg, 7);
   LCD_BIT_OUT (lcd_data_neg, 8);
   LCD_BIT_OUT (lcd_data_neg, 9);
   LCD_BIT_OUT (lcd_data_neg, 10);
   LCD_BIT_OUT (lcd_data_neg, 11);
   LCD_BIT_OUT (lcd_data_neg, 12);
   LCD_BIT_OUT (lcd_data_neg, 13);
   LCD_BIT_OUT (lcd_data_neg, 14);
   LCD_BIT_OUT (lcd_data_neg, 15);

   // extra clock cycle to shift the last bits out + enable output
   LCD_PORT = _BV (LCD_OE_); // LCD_CLK low phase, output enable still off
   LCD_PORT = _BV (LCD_COM) | _BV (LCD_CLK); // LCD_CLK high phase, output enable ON
}

#undef LCD_BIT_OUT
