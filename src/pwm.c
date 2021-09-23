#include "common.h"
#include "pwm.h"

#include <avr/io.h>
#include <avr/power.h>

#define PIEZO_RESOLUTION 32
#define PIEZO_DIV (PIEZO_RESOLUTION - 1)

#define PIN_LED_R   PD3
#define PWM_LED_R   OCR2B

#define PIN_LED_G   PD5
#define PWM_LED_G   OCR0B

#define PIN_LED_B   PB3
#define PWM_LED_B   OCR2A

#define PIN_PIEZO   PD6
#define PWM_PIEZO   OCR0A

static uint8_t pwm_led [3];

void
pwm_setup ()
{
   power_timer0_enable ();
   power_timer2_enable ();

   // Enable output pins
   DDRD |= _BV (PIN_LED_R) | _BV (PIN_LED_G) | _BV (PIN_PIEZO);
   DDRB |= _BV (PIN_LED_B);

   // Timer0: PWM for Buzzer and LED
   // Fast PWM mode:
   // - Toggle OC0A on Compare Match to drive Piezo
   // - Non-inverting PWM on OC0B for driving LED
   TCCR0A = _BV (COM0A0) | _BV (COM0B1) | _BV (COM0B0) |
            _BV (WGM01) | _BV (WGM00);
   TCCR0B = _BV (WGM02) | _BV (CS00); // No clock prescaler clk/1
   PWM_PIEZO = PIEZO_DIV; // buzzer freq: F_CPU/2/(PIEZO_DIV+1)

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
   case IDX_LED_R: PWM_LED_R = 255 - amount; break;
   case IDX_LED_G: PWM_LED_G = PIEZO_DIV - amount / (256/PIEZO_RESOLUTION); break;
   case IDX_LED_B: PWM_LED_B = 255 - amount; break;
   default: break;
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
