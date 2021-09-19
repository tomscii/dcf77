#include "defines.h"
#include "usart.h"

int
usart_putchar (char c, FILE* stream)
{
   // put stuff into the tx ringbuffer instead of looping
   loop_until_bit_is_set (UCSR0A, UDRE0);
   UDR0 = c;

   return 0;
}

int
usart_getchar (FILE* stream)
{
   uint8_t c;

   loop_until_bit_is_set (UCSR0A, RXC0);

   if (UCSR0A & _BV (FE0))
      return _FDEV_EOF;

   if (UCSR0A & _BV (DOR0))
      return _FDEV_ERR;

   c = UDR0;

   return c;
}
