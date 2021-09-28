#include "common.h"
#include "clock.h"
#include "dht22.h"
#include "lcd.h"
#include "pwm.h"
#include "usart.h"
#include "utils.h"

#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/sleep.h>

#include <util/delay.h>

uint16_t lcd_data [] = { 0, 0, 0 };

#define INPUT_BUFSIZE 16
char input [INPUT_BUFSIZE];
uint8_t input_count;

void
ioinit (void)
{
   // set up clock prescaler for MCU clock
   CLKPR = _BV (CLKPCE);
   //CLKPR = _BV (CLKPS0);                  //  /2 -> 1 MHz
   CLKPR = _BV (CLKPS1) | _BV (CLKPS0);   //  /8 -> 250 kHz
   //CLKPR = _BV (CLKPS2);                  // /16 -> 125 kHz

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

   clock_setup ();
   usart_setup ();
   lcd_setup ();
   pwm_setup ();
   dht22_setup ();

   DEBUG_LED_ENABLE_OUTPUT;

   sei ();
}

void
process_input ()
{
   switch (input [input_count - 1])
   {
   case '\b': // Backspace key
      input_count = 0;
      break;
   case 's': // set time
      if (input_count == 5)  // HHMMs
      {
         uint8_t hours = 10 * (input [0] - '0') + input [1] - '0';
         uint8_t minutes = 10 * (input [2] - '0') + input [3] - '0';
         clock_set (hours, minutes, 0);
      }
      else if (input_count == 7)  // HHMMSSs
      {
         uint8_t hours = 10 * (input [0] - '0') + input [1] - '0';
         uint8_t minutes = 10 * (input [2] - '0') + input [3] - '0';
         uint8_t seconds = 10 * (input [4] - '0') + input [5] - '0';
         clock_set (hours, minutes, seconds);
      }
      input_count = 0;
      break;
   case 't': // nudge time forward
      clock_plus_jiffy ();
      input_count = 0;
      break;
   case 'T': // nudge time backward
      clock_minus_jiffy ();
      input_count = 0;
      break;
   case 'm': // measure humidity + temperature sensor
      dht22_trigger ();
      _delay_ms (1);
      dht22_read ();
      input_count = 0;
      break;
   case 'w': // DHT22 power on
      dht22_power_set (1);
      input_count = 0;
      break;
   case 'W': // DHT22 power off
      dht22_power_set (0);
      input_count = 0;
      break;
   case 'r': // nudge red channel up
      pwm_nudge_led (IDX_LED_R, 8);
      input_count = 0;
      break;
   case 'R': // nudge red channel down
      pwm_nudge_led (IDX_LED_R, -8);
      input_count = 0;
      break;
   case 'g': // nudge green channel up
      pwm_nudge_led (IDX_LED_G, 8);
      input_count = 0;
      break;
   case 'G': // nudge green channel down
      pwm_nudge_led (IDX_LED_G, -8);
      input_count = 0;
      break;
   case 'b': // nudge blue channel up
      pwm_nudge_led (IDX_LED_B, 8);
      input_count = 0;
      break;
   case 'B': // nudge blue channel down
      pwm_nudge_led (IDX_LED_B, -8);
      input_count = 0;
      break;
   case 'p': // piezo on
      pwm_piezo_set (1);
      input_count = 0;
      break;
   case 'P': // piezo off
      pwm_piezo_set (0);
      input_count = 0;
      break;
   default:
      break;
   }
}

void
main_loop ()
{
   if (clock_flags.tick)
   {
      clock_flags.tick = 0;

      lcd_send_frame ();

      if (clock_flags.ovf_jiffies)
      {
         clock_flags.ovf_jiffies = 0;

         lcd_data [0] = (clock.minutes << 8) + clock.seconds;
         lcd_set_bits (lcd_data);

         put_str (d2 [clock.hours]);
         put_str (":");
         put_str (d2 [clock.minutes]);
         put_str (":");
         put_str (d2 [clock.seconds]);
         put_str ("\r\n");
      }
   }

   if (usart_flags.rx)
   {
      usart_flags.rx = 0;

      int c;
      while ((c = getchar ()) != EOF)
      {
         putchar (c); // echo

         input [input_count] = c;
         if (input_count < INPUT_BUFSIZE - 1)
            ++input_count;

         process_input ();
      }
      clearerr (stdin);
   }
}

int
main ()
{
   ioinit ();

   put_str ("\r\n\r\nDCF77 booted\r\n");
   clock_adjust (3.8301 * 2048);

   for (;;)
   {
      main_loop ();
      sleep_mode ();
   }

   return 0;
}
