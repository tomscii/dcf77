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

/* Connection layout of LCD display module */
struct lcd_bits_t
{
   union
   {
      struct
      {
         uint8_t seg_4dp: 1;
         uint8_t seg_4c: 1;
         uint8_t seg_4d: 1;
         uint8_t seg_4e: 1;
         uint8_t seg_3dp: 1;
         uint8_t seg_3c: 1;
         uint8_t seg_3d: 1;
         uint8_t seg_3e: 1;
         uint8_t seg_2dp: 1;
         uint8_t seg_2c: 1;
         uint8_t seg_2d: 1;
         uint8_t seg_2e: 1;
         uint8_t seg_1dp: 1;
         uint8_t seg_1c: 1;
         uint8_t seg_1d: 1;
         uint8_t seg_1e: 1;
      } s0;
      uint16_t raw_0;
   };

   union
   {
      struct
      {
         uint8_t seg_col: 1;
         uint8_t seg_5g: 1;
         uint8_t seg_5f: 1;
         uint8_t seg_5a: 1;
         uint8_t seg_5b: 1;
         uint8_t seg_6g: 1;
         uint8_t seg_6f: 1;
         uint8_t seg_6a: 1;
         uint8_t seg_6b: 1;
         uint8_t seg_6c: 1;
         uint8_t seg_6d: 1;
         uint8_t seg_6e: 1;
         uint8_t seg_5dp: 1;
         uint8_t seg_5c: 1;
         uint8_t seg_5d: 1;
         uint8_t seg_5e: 1;
      } s1;
      uint16_t raw_1;
   };

   union
   {
      struct
      {
         uint8_t seg_1g: 1;
         uint8_t seg_1f: 1;
         uint8_t seg_1a: 1;
         uint8_t seg_1b: 1;
         uint8_t seg_2g: 1;
         uint8_t seg_2f: 1;
         uint8_t seg_2a: 1;
         uint8_t seg_2b: 1;
         uint8_t seg_3g: 1;
         uint8_t seg_3f: 1;
         uint8_t seg_3a: 1;
         uint8_t seg_3b: 1;
         uint8_t seg_4g: 1;
         uint8_t seg_4f: 1;
         uint8_t seg_4a: 1;
         uint8_t seg_4b: 1;
      } s2;
      uint16_t raw_2;
   };
};
struct lcd_bits_t lcd_bits;
uint16_t* const lcd_data = (uint16_t *)&lcd_bits;

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
 * Both need to be maintained on display content changes,
 * see lcd_commit ().
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

   // initialize for power-on display test
   lcd_bits.raw_0 = 0xffff;
   lcd_bits.raw_1 = 0xffff;
   lcd_bits.raw_2 = 0xffff;
}

void
lcd_commit ()
{
   uint8_t j = 0;
   for (j = 0; j < 16; ++j)
   {
      uint16_t m = 1 << j;
      uint8_t k = 0;
      for (k = 0; k < 3; ++k)
      {
         uint8_t n = 1 << k;
         if (lcd_data [k] & m)
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

// Return 7-segment bits as 0gfedcba
uint8_t
decode_segments (char c)
{
   switch (c)
   {
   case ' ': return 0x00;
   case '0': return 0x3f;
   case '1': return 0x06;
   case '2': return 0x5b;
   case '3': return 0x4f;
   case '4': return 0x66;
   case '5': return 0x6d;
   case '6': return 0x7d;
   case '7': return 0x07;
   case '8': return 0x7f;
   case '9': return 0x6f;
   }
   return 0xff;
}

void
lcd_set_digit (uint8_t idx, char c)
{
   uint8_t seg = decode_segments (c);
   switch (idx)
   {
   case 1:
      lcd_bits.s2.seg_1a = seg & 1;
      lcd_bits.s2.seg_1b = (seg & (1 << 1)) >> 1;
      lcd_bits.s0.seg_1c = (seg & (1 << 2)) >> 2;
      lcd_bits.s0.seg_1d = (seg & (1 << 3)) >> 3;
      lcd_bits.s0.seg_1e = (seg & (1 << 4)) >> 4;
      lcd_bits.s2.seg_1f = (seg & (1 << 5)) >> 5;
      lcd_bits.s2.seg_1g = (seg & (1 << 6)) >> 6;
      break;
   case 2:
      lcd_bits.s2.seg_2a = seg & 1;
      lcd_bits.s2.seg_2b = (seg & (1 << 1)) >> 1;
      lcd_bits.s0.seg_2c = (seg & (1 << 2)) >> 2;
      lcd_bits.s0.seg_2d = (seg & (1 << 3)) >> 3;
      lcd_bits.s0.seg_2e = (seg & (1 << 4)) >> 4;
      lcd_bits.s2.seg_2f = (seg & (1 << 5)) >> 5;
      lcd_bits.s2.seg_2g = (seg & (1 << 6)) >> 6;
      break;
   case 3:
      lcd_bits.s2.seg_3a = seg & 1;
      lcd_bits.s2.seg_3b = (seg & (1 << 1)) >> 1;
      lcd_bits.s0.seg_3c = (seg & (1 << 2)) >> 2;
      lcd_bits.s0.seg_3d = (seg & (1 << 3)) >> 3;
      lcd_bits.s0.seg_3e = (seg & (1 << 4)) >> 4;
      lcd_bits.s2.seg_3f = (seg & (1 << 5)) >> 5;
      lcd_bits.s2.seg_3g = (seg & (1 << 6)) >> 6;
      break;
   case 4:
      lcd_bits.s2.seg_4a = seg & 1;
      lcd_bits.s2.seg_4b = (seg & (1 << 1)) >> 1;
      lcd_bits.s0.seg_4c = (seg & (1 << 2)) >> 2;
      lcd_bits.s0.seg_4d = (seg & (1 << 3)) >> 3;
      lcd_bits.s0.seg_4e = (seg & (1 << 4)) >> 4;
      lcd_bits.s2.seg_4f = (seg & (1 << 5)) >> 5;
      lcd_bits.s2.seg_4g = (seg & (1 << 6)) >> 6;
      break;
   case 5:
      lcd_bits.s1.seg_5a = seg & 1;
      lcd_bits.s1.seg_5b = (seg & (1 << 1)) >> 1;
      lcd_bits.s1.seg_5c = (seg & (1 << 2)) >> 2;
      lcd_bits.s1.seg_5d = (seg & (1 << 3)) >> 3;
      lcd_bits.s1.seg_5e = (seg & (1 << 4)) >> 4;
      lcd_bits.s1.seg_5f = (seg & (1 << 5)) >> 5;
      lcd_bits.s1.seg_5g = (seg & (1 << 6)) >> 6;
      break;
   case 6:
      lcd_bits.s1.seg_6a = seg & 1;
      lcd_bits.s1.seg_6b = (seg & (1 << 1)) >> 1;
      lcd_bits.s1.seg_6c = (seg & (1 << 2)) >> 2;
      lcd_bits.s1.seg_6d = (seg & (1 << 3)) >> 3;
      lcd_bits.s1.seg_6e = (seg & (1 << 4)) >> 4;
      lcd_bits.s1.seg_6f = (seg & (1 << 5)) >> 5;
      lcd_bits.s1.seg_6g = (seg & (1 << 6)) >> 6;
      break;
   }
}

void
lcd_set_dot (uint8_t idx, char c)
{
   switch (idx)
   {
   case 1:
      lcd_bits.s0.seg_1dp = c & 1;
      break;
   case 2:
      lcd_bits.s0.seg_2dp = c & 1;
      break;
   case 3:
      lcd_bits.s0.seg_3dp = c & 1;
      break;
   case 4:
      lcd_bits.s0.seg_4dp = c & 1;
      break;
   case 5:
      lcd_bits.s1.seg_5dp = c & 1;
      break;
   }
}

void lcd_set_colons (char on)
{
   lcd_bits.s1.seg_col = on & 1;
}

void
lcd_set_dot5_immediate (char c)
{
   lcd_bits.s1.seg_5dp = c & 1;

   if (c)
   {
      lcd_data_pos [12] |= 0x2;
      lcd_data_neg [12] &= ~0x2;
   }
   else
   {
      lcd_data_pos [12] &= ~0x2;
      lcd_data_neg [12] |= 0x2;
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
