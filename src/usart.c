#include "common.h"
#include "usart.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>

#define USART_BAUD 1200UL

FILE usart_str = FDEV_SETUP_STREAM (usart_putchar, usart_getchar,
                                    _FDEV_SETUP_RW);

volatile struct usart_flags_t usart_flags;

#define BUFSIZE 64 /* MUST be integer power of 2, max. 256 */
#define BUFMASK (BUFSIZE-1)

char txbuf [BUFSIZE];
uint8_t txrp = 0;
uint8_t txwp = 0;

char rxbuf [BUFSIZE];
uint8_t rxrp = 0;
uint8_t rxwp = 0;

ISR (USART_RX_vect)
{
   rxbuf [rxwp & BUFMASK] = UDR0;
   ++rxwp;
   usart_flags.rx = 1;
}

ISR (USART_UDRE_vect)
{
   if (txrp != txwp)
   {
      UDR0 = txbuf [txrp & BUFMASK];
      ++txrp;
   }
   else
   {
      UCSR0B &= ~_BV (UDRIE0);
      DEBUG_LED_TOGGLE;
   }
}

void
usart_setup ()
{
   power_usart0_enable ();

   UCSR0A = _BV (U2X0);         // improve precision @ low clock rate
   UCSR0B = _BV (RXCIE0) | _BV (RXEN0) | _BV (TXEN0);
   UCSR0C = _BV (UCSZ01) | _BV (UCSZ00);
   UBRR0H = 0;
   UBRR0L = F_CPU / 8 / USART_BAUD - 1;

   stderr = stdout = stdin = &usart_str;
}

int
usart_putchar (char c, FILE* stream)
{
   txbuf [txwp & BUFMASK] = c;
   ++txwp;
   UCSR0B |= _BV (UDRIE0);
   return 0;
}

int
usart_getchar (FILE* stream)
{
   if (rxrp != rxwp)
   {
      char c = rxbuf [rxrp & BUFMASK];
      ++rxrp;
      return c;
   }
   else
      return _FDEV_EOF;
}
