#include "common.h"
#include "usart.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>

FILE usart_str = FDEV_SETUP_STREAM (usart_putchar, usart_getchar,
                                    _FDEV_SETUP_RW);

#define BUFSIZE 32 /* MUST be integer power of 2 */
#define BUFMASK (BUFSIZE-1)
char txbuf [BUFSIZE];
uint8_t txrp = 0;
uint8_t txwp = 0;

ISR (USART_RX_vect)
{
   char c;

   c = UDR0;
   if (bit_is_clear (UCSR0A, FE0))
   {
      UDR0 = c; // echo
#if 0
      rx[rcv_cnt++] = c;
      if (rcv_cnt == 4)
      {
         hours = 10 * (rx[0] - '0') + rx[1] - '0';
         minutes = 10 * (rx[2] - '0') + rx[3] - '0';
         seconds = 0;
         jiffies = 0;
         TCNT1 = 0;
      }
#endif
   }
}

ISR (USART_TX_vect)
{
   if (txrp != txwp)
   {
      UDR0 = txbuf [txrp & BUFMASK];
      ++txrp;
   }
   else
      DEBUG_LED_TOGGLE;
}

void
usart_setup ()
{
   power_usart0_enable ();

   UCSR0A = _BV (U2X0);         // improve precision @ low clock rate
   UCSR0B = _BV (RXCIE0) | _BV (TXCIE0) | _BV (RXEN0) | _BV (TXEN0);
   UCSR0C = _BV (UCSZ01) | _BV (UCSZ00);
   UBRR0H = 0;
   UBRR0L = F_CPU / 8 / UART_BAUD - 1;
}

int
usart_putchar (char c, FILE* stream)
{
   cli ();

   if ((txwp == txrp) && (UCSR0A & _BV (UDRE0)))
   {
      UDR0 = c;
   }
   else
   {
      txbuf [txwp & BUFMASK] = c;
      ++txwp;
   }

   sei ();
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
