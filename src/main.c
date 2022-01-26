#include "backlight.h"
#include "clock.h"
#include "common.h"
#include "dcf77.h"
#include "dht22.h"
#include "lcd.h"
#include "pbtn.h"
#include "pwm.h"
#include "usart.h"
#include "utils.h"

#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/sleep.h>

enum screen_t
{
   S_TIME_OF_DAY = 0, S_DATE, S_DAY_OF_WEEK, S_TEMPERATURE, S_HUMIDITY,
   S_COUNT
};
enum screen_t screen = S_TIME_OF_DAY;
char display_full_update = 1;

#define INPUT_BUFSIZE 16
char input [INPUT_BUFSIZE];
uint8_t input_count;

char echo = 0;

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

   usart_setup (); // this is first to allow printouts in later setup functions
   clock_setup ();
   dcf77_setup ();
   dht22_setup ();
   lcd_setup ();
   pbtn_setup ();
   pwm_setup ();

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
         clock_set_hms (hours, minutes, 0);
      }
      else if (input_count == 7)  // HHMMSSs
      {
         uint8_t hours = 10 * (input [0] - '0') + input [1] - '0';
         uint8_t minutes = 10 * (input [2] - '0') + input [3] - '0';
         uint8_t seconds = 10 * (input [4] - '0') + input [5] - '0';
         clock_set_hms (hours, minutes, seconds);
      }
      input_count = 0;
      break;
   case 'a': // clock adjust (test)
      clock_adjust (1000);
      input_count = 0;
      break;
   case 'A': // clock adjust (test)
      clock_adjust (-200);
      input_count = 0;
      break;
   case 'Z': // zero out clock adjustment
      clock_adjust_reset ();
      input_count = 0;
      break;
   case 'e': // echo on
      echo = 1;
      input_count = 0;
      break;
   case 'E': // echo off
      echo = 0;
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
      dht22_schedule ();
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
   case 'v': // DCF77 power on
      dcf77_power_set (1);
      input_count = 0;
      break;
   case 'V': // DCF77 power off
      dcf77_power_set (0);
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
display_tod ()
{
   if (clock_flags.ovf_minutes || display_full_update)
   {
      clock_flags.ovf_minutes = 0;

      if (clock.hours < 10)
         lcd_set_digit (1, ' ');
      else
         lcd_set_digit (1, '0' + (clock.hours / 10));
      lcd_set_digit (2, '0' + (clock.hours % 10));
   }
   if (clock_flags.ovf_seconds || display_full_update)
   {
      clock_flags.ovf_seconds = 0;

      lcd_set_digit (3, '0' + (clock.minutes / 10));
      lcd_set_digit (4, '0' + (clock.minutes % 10));
   }

   lcd_set_digit (5, '0' + (clock.seconds / 10));
   lcd_set_digit (6, '0' + (clock.seconds % 10));

   lcd_set_dot (2, clock.dst);

   if (display_full_update)
   {
      lcd_set_dot (1, 0);
      lcd_set_dot (3, 0);
      lcd_set_dot (4, 0);
      lcd_set_colons (1);
   }
}

void
display_date ()
{
   if (clock_flags.ovf_months || display_full_update)
   {
      clock_flags.ovf_months = 0;

      lcd_set_digit (1, '0' + (clock.year / 10));
      lcd_set_digit (2, '0' + (clock.year % 10));
   }
   if (clock_flags.ovf_days || display_full_update)
   {
      clock_flags.ovf_days = 0;

      lcd_set_digit (3, '0' + (clock.month / 10));
      lcd_set_digit (4, '0' + (clock.month % 10));
   }
   if (clock_flags.ovf_hours || display_full_update)
   {
      clock_flags.ovf_hours = 0;

      lcd_set_digit (5, '0' + (clock.day / 10));
      lcd_set_digit (6, '0' + (clock.day % 10));
   }
   if (display_full_update)
   {
      lcd_set_dot (1, 0);
      lcd_set_dot (2, 1);
      lcd_set_dot (3, 0);
      lcd_set_dot (4, 1);
      lcd_set_colons (0);
   }
}

void
display_dow ()
{
   if (clock_flags.ovf_hours || display_full_update)
   {
      clock_flags.ovf_hours = 0;

      lcd_set_digit (4, '0' + (clock.dow));
   }
   if (display_full_update)
   {
      lcd_set_digit (1, ' ');
      lcd_set_digit (2, ' ');
      lcd_set_digit (3, 'd');
      lcd_set_digit (5, ' ');
      lcd_set_digit (6, ' ');
      lcd_set_dot (1, 0);
      lcd_set_dot (2, 0);
      lcd_set_dot (3, 1);
      lcd_set_dot (4, 0);
      lcd_set_colons (0);
   }
}

void
display_temp ()
{
   if (dht22_status.updated || display_full_update)
   {
      dht22_status.updated = 0;

      int16_t temp = dht22_status.temp_last;
      if (temp < -99)
      {
         lcd_set_digit (1, '-');
         temp = -temp;
         lcd_set_digit (2, '0' + temp / 100);
      }
      else if (temp < 0)
      {
         lcd_set_digit (1, ' ');
         lcd_set_digit (2, '-');
         temp = -temp;
      }
      else if (temp < 100)
      {
         lcd_set_digit (1, ' ');
         lcd_set_digit (2, ' ');
      }
      else
      {
         lcd_set_digit (1, ' ');
         lcd_set_digit (2, '0' + temp / 100);
      }
      lcd_set_digit (3, '0' + (temp % 100) / 10);
      lcd_set_digit (4, '0' + (temp % 10));
   }
   if (display_full_update)
   {
      lcd_set_digit (5, '*'); // degree sign
      lcd_set_digit (6, 'C');
      lcd_set_dot (1, 0);
      lcd_set_dot (2, 0);
      lcd_set_dot (3, 1);
      lcd_set_dot (4, 0);
      lcd_set_colons (0);
   }
}

void
display_rh ()
{
   if (dht22_status.updated || display_full_update)
   {
      dht22_status.updated = 0;

      int16_t rh = dht22_status.rh_last;
      if (rh >= 1000)
      {
         lcd_set_digit (1, '1');
         lcd_set_digit (2, '0' + (rh - 1000) / 100);
      }
      else if (rh >= 100)
      {
         lcd_set_digit (1, ' ');
         lcd_set_digit (2, '0' + rh / 100);
      }
      else
      {
         lcd_set_digit (1, ' ');
         lcd_set_digit (2, ' ');
      }

      lcd_set_digit (3, '0' + (rh % 100) / 10);
      lcd_set_digit (4, '0' + (rh % 10));
   }
   if (display_full_update)
   {
      lcd_set_digit (5, 'r');
      lcd_set_digit (6, 'h');
      lcd_set_dot (1, 0);
      lcd_set_dot (2, 0);
      lcd_set_dot (3, 1);
      lcd_set_dot (4, 0);
      lcd_set_colons (0);
   }
}

void
display_blank ()
{
   if (display_full_update)
   {
      lcd_set_digit (1, ' ');
      lcd_set_digit (2, ' ');
      lcd_set_digit (3, ' ');
      lcd_set_digit (4, ' ');
      lcd_set_digit (5, ' ');
      lcd_set_digit (6, ' ');
      lcd_set_dot (1, 0);
      lcd_set_dot (2, 0);
      lcd_set_dot (3, 0);
      lcd_set_dot (4, 0);
      lcd_set_colons (0);
   }
}

void
display_update ()
{
   switch (screen)
   {
   case S_TIME_OF_DAY: display_tod (); break;
   case S_DATE:        display_date (); break;
   case S_DAY_OF_WEEK: display_dow (); break;
   case S_TEMPERATURE: display_temp (); break;
   case S_HUMIDITY:    display_rh (); break;
   default: display_blank (); break;
   }

   display_full_update = 0;

}

void
process_event (uint8_t evt)
{
   if (evt & F_EVT_LONG)
   {
      if ((evt & F_BTN_MASK) == (F_BTN_MODE | F_BTN_ADJ))
      {
         dcf77_schedule_sync ();
         backlight_set (0); // cut PWM interference to DCF receiver
         return;
      }
   }
   else if (evt & F_EVT_REGULAR)
   {
      if ((evt & F_BTN_MASK) == F_BTN_MODE)
      {
         if (backlight_set (1))
         {
            // backlight was already on

            if (++screen == S_COUNT)
               screen = 0;

            dht22_measure_interval (screen == S_TEMPERATURE || screen == S_HUMIDITY);

            display_full_update = 1;
         }
         return;
      }
   }

   // currently unhandled:
   put_str ("evt=");
   put_byte_hex (evt);
   put_str ("\r\n");
}

void
main_loop ()
{
   if (clock_flags.tick)
   {
      dcf77_on_tick ();
      pbtn_on_tick ();

      if (clock_flags.ovf_jiffies)
      {
         dcf77_on_second ();

         if (echo)
         {
            put_str (d2 [(uint8_t)clock.hours]);
            put_str (":");
            put_str (d2 [(uint8_t)clock.minutes]);
            put_str (":");
            put_str (d2 [(uint8_t)clock.seconds]);
            put_str ("\r\n");
         }
      }
      display_update ();
      lcd_send_frame ();

      dht22_on_tick ();
      if (clock_flags.ovf_jiffies)
      {
         dht22_on_second ();
      }

      backlight_on_tick ();
      clock_flags.ovf_jiffies = 0;
   }
   clock_flags.tick = 0;

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

   if (!pbtn_flags.ack_event && pbtn_flags.pending_event)
   {
      process_event (pbtn_flags.pending_event);
      pbtn_flags.ack_event = 1;
   }
}

int
main ()
{
   ioinit ();

   put_str ("DCF77 booted\r\n");

   for (;;)
   {
      main_loop ();
      sleep_mode ();
   }

   return 0;
}
