#include "defines.h"
#include "usart.h"

#include <inttypes.h>
#include <stdio.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <avr/sleep.h>

#define F_HZ 50
#define CLKDIV (F_CPU/F_HZ)

uint16_t seconds;
uint8_t jiffy;

// Bits set inside ISRs, watched and reset in main loop
volatile struct
{
  uint8_t timer1_int: 1;
}
intflags;

FILE usart_str = FDEV_SETUP_STREAM (usart_putchar, usart_getchar, _FDEV_SETUP_RW);

ISR (TIMER1_OVF_vect)
{
   if (++jiffy == F_HZ)
      jiffy = 0;
   intflags.timer1_int = 1;
}

ISR (USART_RX_vect)
{
   uint8_t c;

   c = UDR0;
   if (bit_is_clear (UCSR0A, FE0))
   {
      UDR0 = c; // echo
   }
}

ISR (USART_TX_vect)
{
   PORTC ^= _BV (PC5);
}

void
ioinit (void)
{
   // set up clock prescaler for MCU clock
   CLKPR = _BV (CLKPCE);
   //CLKPR = _BV (CLKPS0);                  //  /2 -> 1 MHz
   //CLKPR = _BV (CLKPS1) | _BV (CLKPS0);   //  /8 -> 250 kHz
   CLKPR = _BV (CLKPS2);                  // /16 -> 125 kHz

   // set all IO pins as input, pull-up active
   PORTB = 0xff;
   DDRB = 0;
   PORTC = 0xff;
   DDRC = 0;
   PORTD = 0xff;
   DDRD = 0;

   // disable unused peripherals to save power
   ADCSRA &= ~_BV (ADEN);       // disable ADC
   ACSR |= _BV (ACD);           // disable Analog Comparator
   SPCR &= ~_BV (SPE);          // disable SPI
   TWCR &= ~_BV (TWEN);         // disable TWI

   power_all_disable ();
   power_timer1_enable ();
   power_usart0_enable ();

   // Timer 1 is 10-bit PWM
   TCCR1A = _BV (WGM11) | _BV (WGM10);
   TCCR1B = _BV (WGM13) | _BV (WGM12) | _BV (CS10);
   OCR1A = CLKDIV;
   TIMSK1 = _BV (TOIE1);

   // USART setup
   UCSR0A = _BV (U2X0);         // improve precision @ low clock rate
   UCSR0B = _BV (RXCIE0) | _BV (TXCIE0) | _BV (RXEN0) | _BV (TXEN0);
   UCSR0C = _BV (UCSZ01) | _BV (UCSZ00);
   UBRR0H = 0;
   UBRR0L = F_CPU / 8 / UART_BAUD - 1;

   // Enable LED pin as output
   DDRC |= _BV (PC5);

   // PC0..4: LCD DATA0, DATA1, CLK, COM, OE/
   DDRC |= _BV (PC0) | _BV (PC1) | _BV (PC2) | _BV (PC3) | _BV (PC4);
   PORTC |= _BV (PC2) | _BV (PC4); // active low, init to high
   PORTC &= ~(_BV (PC0) | _BV (PC1) | _BV (PC3));

   sei ();
}

void
main_loop (void)
{
   if (intflags.timer1_int)
   {
      intflags.timer1_int = 0;

      if (jiffy == 0)
      {
       #if 1
         cli();
         uint16_t t0 = TCNT1;
         sei();
       #endif

         // shift the bits of current seconds counter out on DATA0
         uint8_t j;
         PORTC &= ~_BV (PC2); // clk fall (kept high when idle)
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

         // real data
         for (j = 0; j < 16; ++j)
         {
            if (seconds & 1U)
               PORTC |= _BV (PC0);
            else
               PORTC &= ~_BV (PC0);

            if (t0 & 1U)
               PORTC |= _BV (PC1);
            else
               PORTC &= ~_BV (PC1);

            PORTC |= _BV (PC2); // clk rise
            PORTC &= ~_BV (PC2); // clk fall
         }

         // an extra shift to latch the last bit
         PORTC |= _BV (PC2); // clk rise and keep high for idle state
         //PORTC &= ~_BV (PC2); // clk fall

         // enable shift register output (until next jiffy)
         PORTC &= ~_BV (PC4);

       #if 0
         cli();
         uint16_t t1 = TCNT1;
         sei();
         t1 += CLKDIV * jiffy;
         printf (" %d", t1 - t0);
         // Results: 439, 455. BUDGET: 500 clock cycles
       #endif

         ++seconds;
         printf (" %d", seconds);
      }
      else
      {
         // disable shift register output
         PORTC |= _BV (PC4);
      }
   }
}

int
main (void)
{
   ioinit ();

   stderr = stdout = stdin = &usart_str;
   printf_P (PSTR ("\r\n\r\nDCF77 booted\r\n"));

   for (;;)
   {
      main_loop ();
      sleep_mode ();
   }

   return 0;
}
