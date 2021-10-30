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

/* Data table for efficient bit-banging our LCD data
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
 */
struct lcd_bits_t
{
#define SEGBITS(N, D0, D1, D2)                                          \
   uint8_t D0: 1;                                                       \
   uint8_t D1: 1;                                                       \
   uint8_t D2: 1;                                                       \
   uint8_t fill_##N: 5

   //           D0       D1       D2
   SEGBITS (0,  seg_4dp, seg_col, seg_1g);
   SEGBITS (1,  seg_4c,  seg_5g,  seg_1f);
   SEGBITS (2,  seg_4d,  seg_5f,  seg_1a);
   SEGBITS (3,  seg_4e,  seg_5a,  seg_1b);
   SEGBITS (4,  seg_3dp, seg_5b,  seg_2g);
   SEGBITS (5,  seg_3c,  seg_6g,  seg_2f);
   SEGBITS (6,  seg_3d,  seg_6f,  seg_2a);
   SEGBITS (7,  seg_3e,  seg_6a,  seg_2b);
   SEGBITS (8,  seg_2dp, seg_6b,  seg_3g);
   SEGBITS (9,  seg_2c,  seg_6c,  seg_3f);
   SEGBITS (10, seg_2d,  seg_6d,  seg_3a);
   SEGBITS (11, seg_2e,  seg_6e,  seg_3b);
   SEGBITS (12, seg_1dp, seg_5dp, seg_4g);
   SEGBITS (13, seg_1c,  seg_5c,  seg_4f);
   SEGBITS (14, seg_1d,  seg_5d,  seg_4a);
   SEGBITS (15, seg_1e,  seg_5e,  seg_4b);

#undef SEGBITS
};
struct lcd_bits_t lcd_bits;
uint8_t* const lcd_data = (uint8_t *)&lcd_bits;

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
   for (uint8_t j = 0; j < 16; ++j)
      lcd_data [j] = 0x27;
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
      lcd_bits.seg_1a = seg & 1;
      lcd_bits.seg_1b = (seg & (1 << 1)) >> 1;
      lcd_bits.seg_1c = (seg & (1 << 2)) >> 2;
      lcd_bits.seg_1d = (seg & (1 << 3)) >> 3;
      lcd_bits.seg_1e = (seg & (1 << 4)) >> 4;
      lcd_bits.seg_1f = (seg & (1 << 5)) >> 5;
      lcd_bits.seg_1g = (seg & (1 << 6)) >> 6;
      break;
   case 2:
      lcd_bits.seg_2a = seg & 1;
      lcd_bits.seg_2b = (seg & (1 << 1)) >> 1;
      lcd_bits.seg_2c = (seg & (1 << 2)) >> 2;
      lcd_bits.seg_2d = (seg & (1 << 3)) >> 3;
      lcd_bits.seg_2e = (seg & (1 << 4)) >> 4;
      lcd_bits.seg_2f = (seg & (1 << 5)) >> 5;
      lcd_bits.seg_2g = (seg & (1 << 6)) >> 6;
      break;
   case 3:
      lcd_bits.seg_3a = seg & 1;
      lcd_bits.seg_3b = (seg & (1 << 1)) >> 1;
      lcd_bits.seg_3c = (seg & (1 << 2)) >> 2;
      lcd_bits.seg_3d = (seg & (1 << 3)) >> 3;
      lcd_bits.seg_3e = (seg & (1 << 4)) >> 4;
      lcd_bits.seg_3f = (seg & (1 << 5)) >> 5;
      lcd_bits.seg_3g = (seg & (1 << 6)) >> 6;
      break;
   case 4:
      lcd_bits.seg_4a = seg & 1;
      lcd_bits.seg_4b = (seg & (1 << 1)) >> 1;
      lcd_bits.seg_4c = (seg & (1 << 2)) >> 2;
      lcd_bits.seg_4d = (seg & (1 << 3)) >> 3;
      lcd_bits.seg_4e = (seg & (1 << 4)) >> 4;
      lcd_bits.seg_4f = (seg & (1 << 5)) >> 5;
      lcd_bits.seg_4g = (seg & (1 << 6)) >> 6;
      break;
   case 5:
      lcd_bits.seg_5a = seg & 1;
      lcd_bits.seg_5b = (seg & (1 << 1)) >> 1;
      lcd_bits.seg_5c = (seg & (1 << 2)) >> 2;
      lcd_bits.seg_5d = (seg & (1 << 3)) >> 3;
      lcd_bits.seg_5e = (seg & (1 << 4)) >> 4;
      lcd_bits.seg_5f = (seg & (1 << 5)) >> 5;
      lcd_bits.seg_5g = (seg & (1 << 6)) >> 6;
      break;
   case 6:
      lcd_bits.seg_6a = seg & 1;
      lcd_bits.seg_6b = (seg & (1 << 1)) >> 1;
      lcd_bits.seg_6c = (seg & (1 << 2)) >> 2;
      lcd_bits.seg_6d = (seg & (1 << 3)) >> 3;
      lcd_bits.seg_6e = (seg & (1 << 4)) >> 4;
      lcd_bits.seg_6f = (seg & (1 << 5)) >> 5;
      lcd_bits.seg_6g = (seg & (1 << 6)) >> 6;
      break;
   }
}

void
lcd_set_dot (uint8_t idx, char c)
{
   switch (idx)
   {
   case 1:
      lcd_bits.seg_1dp = c & 1;
      break;
   case 2:
      lcd_bits.seg_2dp = c & 1;
      break;
   case 3:
      lcd_bits.seg_3dp = c & 1;
      break;
   case 4:
      lcd_bits.seg_4dp = c & 1;
      break;
   case 5:
      lcd_bits.seg_5dp = c & 1;
      break;
   }
}

void lcd_set_colons (char on)
{
   lcd_bits.seg_col = on & 1;
}

// Runtime: 54 (pos) / 63 (neg) MCU clocks
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
#define LCD_BIT_OUT_POS(arr, n)                     \
   LCD_PORT = arr [n];                              \
   LCD_PORT |= _BV (LCD_CLK);

// unrolled loop for ultimate speed (6 MCU clocks per LCD clock cycle)
#define LCD_BIT_OUT_NEG(arr, n)                     \
   LCD_PORT = arr [n] ^ 0x07;                       \
   LCD_PORT |= _BV (LCD_CLK);

void
lcd_send_frame_pos ()
{
   LCD_BIT_OUT_POS (lcd_data, 0);
   LCD_BIT_OUT_POS (lcd_data, 1);
   LCD_BIT_OUT_POS (lcd_data, 2);
   LCD_BIT_OUT_POS (lcd_data, 3);
   LCD_BIT_OUT_POS (lcd_data, 4);
   LCD_BIT_OUT_POS (lcd_data, 5);
   LCD_BIT_OUT_POS (lcd_data, 6);
   LCD_BIT_OUT_POS (lcd_data, 7);
   LCD_BIT_OUT_POS (lcd_data, 8);
   LCD_BIT_OUT_POS (lcd_data, 9);
   LCD_BIT_OUT_POS (lcd_data, 10);
   LCD_BIT_OUT_POS (lcd_data, 11);
   LCD_BIT_OUT_POS (lcd_data, 12);
   LCD_BIT_OUT_POS (lcd_data, 13);
   LCD_BIT_OUT_POS (lcd_data, 14);
   LCD_BIT_OUT_POS (lcd_data, 15);

   // extra clock cycle to shift the last bits out + enable output
   LCD_PORT = _BV (LCD_OE_); // LCD_CLK low phase, output enable still off
   LCD_PORT = _BV (LCD_CLK); // LCD_CLK high phase, output enable ON
}
void
lcd_send_frame_neg ()
{
   LCD_BIT_OUT_NEG (lcd_data, 0);
   LCD_BIT_OUT_NEG (lcd_data, 1);
   LCD_BIT_OUT_NEG (lcd_data, 2);
   LCD_BIT_OUT_NEG (lcd_data, 3);
   LCD_BIT_OUT_NEG (lcd_data, 4);
   LCD_BIT_OUT_NEG (lcd_data, 5);
   LCD_BIT_OUT_NEG (lcd_data, 6);
   LCD_BIT_OUT_NEG (lcd_data, 7);
   LCD_BIT_OUT_NEG (lcd_data, 8);
   LCD_BIT_OUT_NEG (lcd_data, 9);
   LCD_BIT_OUT_NEG (lcd_data, 10);
   LCD_BIT_OUT_NEG (lcd_data, 11);
   LCD_BIT_OUT_NEG (lcd_data, 12);
   LCD_BIT_OUT_NEG (lcd_data, 13);
   LCD_BIT_OUT_NEG (lcd_data, 14);
   LCD_BIT_OUT_NEG (lcd_data, 15);

   // extra clock cycle to shift the last bits out + enable output
   LCD_PORT = _BV (LCD_OE_); // LCD_CLK low phase, output enable still off
   LCD_PORT = _BV (LCD_COM) | _BV (LCD_CLK); // LCD_CLK high phase, output enable ON
}

#undef LCD_BIT_OUT_POS
#undef LCD_BIT_OUT_NEG
