#include "defines.h"
#include "usart.h"
#include "utils.h"

#include <inttypes.h>
#include <stdio.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <avr/sleep.h>

#define F_TICK 50
#define TICK_DIV (F_CPU/F_TICK - 1)
#define PIEZO_HZ 2500
#define PIEZO_DIV (F_CPU/2/PIEZO_HZ - 1)

uint8_t jiffies = 0;
uint8_t seconds = 0;
uint8_t minutes = 9;
uint8_t hours = 12;

// Bits set inside ISRs, watched and reset in main loop
volatile struct
{
  uint8_t timer1_int: 1;
}
intflags;

ISR (TIMER1_OVF_vect)
{
   if (++jiffies == F_TICK)
      jiffies = 0;
   intflags.timer1_int = 1;
}

ISR (TIMER1_COMPB_vect)
{
   // disable shift register output
   PORTC |= _BV (PC4);
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
   power_timer0_enable ();
   power_timer1_enable ();
   power_timer2_enable ();
   power_usart0_enable ();

   // Timer 1: Fast PWM
   // WGM=15 -> TOP=OCR1A
   // clk prescaler: /1
   TCCR1A = _BV (WGM11) | _BV (WGM10);
   TCCR1B = _BV (WGM13) | _BV (WGM12) | _BV (CS10);
   OCR1A = TICK_DIV;
   OCR1B = 700; // slightly higher than how long the LCD data sending takes
   TIMSK1 = _BV (OCIE1B) | _BV (TOIE1);

   // USART setup
   usart_setup ();

   // Enable LED pin as output
   DDRC |= _BV (PC5);

   // PC0..4: LCD DATA0, DATA1, CLK, COM, OE/
   DDRC |= _BV (PC0) | _BV (PC1) | _BV (PC2) | _BV (PC3) | _BV (PC4);
   PORTC |= _BV (PC2) | _BV (PC4); // active low, init to high
   PORTC &= ~(_BV (PC0) | _BV (PC1) | _BV (PC3));

   // PWM for Buzzer (on PD6) and LED (on PD5)
   DDRD |= _BV (PD5) | _BV (PD6); // enable OC0A and OC0B output pins
   // Fast PWM mode
   // - Toggle OC0A on Compare Match to drive Piezo
   // - Non-inverting PWM on OC0B for driving LED
   TCCR0A = _BV (COM0A0) | _BV (COM0B1) | _BV (WGM01) | _BV (WGM00);
   // No clock prescaler clk/1
   TCCR0B = _BV (WGM02) | _BV (CS00);
   OCR0A = PIEZO_DIV; // buzzer freq
   OCR0B = 0; // LED brightness (0..OCR0A)

   // PWM for LEDs (PB3 and PD3)
   DDRB |= _BV (PB3); // enable OC2A output pin
   DDRD |= _BV (PD3); // enable OC2B output pin
   // Fast PWM, non-inverting on OC2A and OC2B for driving LEDs
   TCCR2A = _BV (COM2A1) | _BV (COM2B1) | _BV (WGM21) | _BV (WGM20);
   // No clock prescaler clk/1
   TCCR2B = _BV (CS20);
   OCR2A = 1;   // LED brightness (0..255)
   OCR2B = 10;  // LED brightness (0..255)

   sei ();
}

void
main_loop (void)
{
   if (intflags.timer1_int)
   {
      intflags.timer1_int = 0;

       #if 0
         cli();
         uint16_t t0 = TCNT1;
         sei();
       #endif

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


       #if 0
         cli();
         uint16_t t1 = TCNT1;
         sei();
         t1 += TICK_DIV * jiffies;
         printf (" %d", t1 - t0);
         // Results: 439, 455. BUDGET: 500 clock cycles
       #endif

      if (++OCR0B == OCR0A)
         OCR0B = 0;

      if (jiffies == 0)
      {
         ++seconds;
         if (seconds == 60)
         {
            seconds = 0;
            ++minutes;
            if (minutes == 60)
            {
               minutes = 0;
               ++hours;
               if (hours == 24)
                  hours = 0;
            }
         }

         putstr ("\r\n");
         putstr (d2 [hours]);
         putstr (":");
         putstr (d2 [minutes]);
         putstr (":");
         putstr (d2 [seconds]);
      }
   }
}

int
main (void)
{
   ioinit ();

   stderr = stdout = stdin = &usart_str;
   puts_P (PSTR ("\r\n\r\nDCF77 booted"));

   for (;;)
   {
      main_loop ();
      sleep_mode ();
   }

   return 0;
}
