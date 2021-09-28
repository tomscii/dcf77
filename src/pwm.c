#include "common.h"
#include "pwm.h"

#include <avr/io.h>
#include <avr/power.h>

#define PIEZO_PWM_BITS 5
#define PIEZO_PWM_COUNT (1 << PIEZO_PWM_BITS)
#define PIEZO_DIV (PIEZO_PWM_COUNT - 1)

#define LED_R_PIN   PD3
#define LED_R_PWM   OCR2B

#define LED_G_PIN   PD5
#define LED_G_PWM   OCR0B

#define LED_B_PIN   PB3
#define LED_B_PWM   OCR2A

#define PIEZO_DDR   DDRD
#define PIEZO_PIN   PD6
#define PIEZO_PWM   OCR0A

static uint8_t pwm_led [3];

void
pwm_setup ()
{
   power_timer0_enable ();
   power_timer2_enable ();

   // Enable output pins
   DDRD |= _BV (LED_R_PIN) | _BV (LED_G_PIN); // piezo initial state is off
   DDRB |= _BV (LED_B_PIN);

   // Timer0: PWM for Buzzer and LED
   // Fast PWM mode:
   // - Toggle OC0A on Compare Match to drive Piezo
   // - Non-inverting PWM on OC0B for driving LED
   TCCR0A = _BV (COM0A0) | _BV (COM0B1) | _BV (COM0B0) |
            _BV (WGM01) | _BV (WGM00);
   TCCR0B = _BV (WGM02) | _BV (CS00); // No clock prescaler clk/1
   PIEZO_PWM = PIEZO_DIV; // buzzer freq: F_CPU/2/PIEZO_PWM_COUNT

   // Timer2: PWM for 2 LEDs
   // Fast PWM, non-inverting on OC2A and OC2B for driving LEDs
   TCCR2A = _BV (COM2A1) | _BV (COM2A0) | _BV (COM2B1) | _BV (COM2B0) |
            _BV (WGM21) | _BV (WGM20);
   TCCR2B = _BV (CS20); // No clock prescaler clk/1

   pwm_set_leds (0, 0, 0);
}

void
pwm_set_leds (uint8_t red, uint8_t green, uint8_t blue)
{
   pwm_set_led (IDX_LED_R, red);
   pwm_set_led (IDX_LED_G, green);
   pwm_set_led (IDX_LED_B, blue);
}

uint8_t
pwm_get_led (uint8_t led_ix)
{
   return pwm_led [led_ix];
}

void
pwm_set_led (uint8_t led_ix, uint8_t amount)
{
   pwm_led [led_ix] = amount;
   switch (led_ix)
   {
   case IDX_LED_R:
      LED_R_PWM = 255 - amount;
      break;
   case IDX_LED_G:
      LED_G_PWM = PIEZO_DIV - (amount >> (8 - PIEZO_PWM_BITS));
      break;
   case IDX_LED_B:
      LED_B_PWM = 255 - amount;
      break;
   default:
      break;
   }
}

void
pwm_nudge_led (uint8_t led_ix, int8_t amount)
{
   int16_t v = pwm_led [led_ix] + amount;
   if (v > 255)
      v = 255;
   else if (v < 0)
      v = 0;
   pwm_set_led (led_ix, v);
}

void pwm_piezo_set (char on)
{
   if (on)
      PIEZO_DDR |= _BV (PIEZO_PIN);
   else
      PIEZO_DDR &= ~_BV (PIEZO_PIN);
}
