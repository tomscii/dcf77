#include "utils.h"

const char* d2[] = {
   "00", "01", "02", "03", "04", "05", "06", "07", "08", "09",
   "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
   "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
   "30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
   "40", "41", "42", "43", "44", "45", "46", "47", "48", "49",
   "50", "51", "52", "53", "54", "55", "56", "57", "58", "59",
};

void
put_str (const char* str)
{
   const char* p = str;
   while (*p)
   {
      putchar (*p);
      ++p;
   }
}

void put_byte_hex (uint8_t x)
{
   static char d [] = { '0', '1', '2', '3', '4', '5', '6', '7',
                        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
   putchar (d [x >> 4]);
   putchar (d [x & 0x0f]);
}

void put_uint (uint16_t x)
{
   char s [6] = { '0', '0', '0', '0', '0', 0 };

   if (x >= 10000)
   {
      s [0] = x / 10000;
      x -= s [0] * 10000;
      s [0] += '0';
   }
   if (x >= 1000)
   {
      s [1] = x / 1000;
      x -= s [1] * 1000;
      s [1] += '0';
   }
   if (x >= 100)
   {
      s [2] = x / 100;
      x -= s [2] * 100;
      s [2] += '0';
   }
   if (x >= 10)
   {
      s [3] = x / 10;
      x -= s [3] * 10;
      s [3] += '0';
   }
   s [4] = '0' + x;

   uint8_t k;
   for (k = 0; k < 4; ++k)
   {
      if (s [k] > '0')
      {
         put_str (s + k);
         return;
      }
   }
   put_str (s + 4);
}

void
put_q4_11 (int16_t x)
{
   int8_t sign = x < 0 ? -1 : 1;
   uint16_t integer = sign * x;
   uint32_t fractional = integer;
   integer &= 0xf800; // upper 5 bits, incl. sign bit
   fractional -= integer;
   integer >>= 11;
   fractional *= 10000;
   fractional /= 2048;

   if (sign < 0)
      putchar ('-');
   put_uint (integer);
   putchar ('.');
   put_uint (fractional);
}

uint8_t n1bits (uint16_t x)
{
   x = (x & 0x5555) + ((x >> 1) & 0x5555);
   x = (x & 0x3333) + ((x >> 2) & 0x3333);
   x = (x & 0x0f0f) + ((x >> 4) & 0x0f0f);
   x = (x & 0x00ff) + ((x >> 8) & 0x00ff);

   return x;
}

uint16_t revbits (uint16_t x)
{
   x = (x & 0x5555) << 1 | (x & 0xAAAA) >> 1;
   x = (x & 0x3333) << 2 | (x & 0xCCCC) >> 2;
   x = (x & 0x0F0F) << 4 | (x & 0xF0F0) >> 4;
   x = (x & 0x00FF) << 8 | (x & 0xFF00) >> 8;

   return x;
}
