#include "common.h"
#include "lcd.h"
#include "pwm.h"
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
   LCD_OUTPUT_DISABLE;
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

   usart_setup ();
   lcd_setup ();
   pwm_setup ();

   // Timer 1: Fast PWM
   // WGM=15 -> TOP=OCR1A
   // clk prescaler: /1
   TCCR1A = _BV (WGM11) | _BV (WGM10);
   TCCR1B = _BV (WGM13) | _BV (WGM12) | _BV (CS10);
   OCR1A = TICK_DIV;
   OCR1B = 700; // slightly higher than how long the LCD data sending takes
   TIMSK1 = _BV (OCIE1B) | _BV (TOIE1);

   DEBUG_LED_ENABLE_OUTPUT;

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

         lcd_send_data ();


       #if 0
         cli();
         uint16_t t1 = TCNT1;
         sei();
         t1 += TICK_DIV * jiffies;
         printf (" %d", t1 - t0);
         // Results: 439, 455. BUDGET: 500 clock cycles
       #endif

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
