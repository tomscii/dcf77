#include "common.h"
#include "pwm.h"

#include <avr/io.h>
#include <avr/power.h>

#define PIEZO_HZ 2500
#define PIEZO_DIV (F_CPU/2/PIEZO_HZ - 1)  // = 24

#define PIN_LED_R   PD3
#define PWM_LED_R   OCR2B

#define PIN_LED_G   PD5
#define PWM_LED_G   OCR0B

#define PIN_LED_B   PB3
#define PWM_LED_B   OCR2A

#define PIN_PIEZO   PD6
#define PWM_PIEZO   OCR0A

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
   TCCR0A = _BV (COM0A0) | _BV (COM0B1) | _BV (WGM01) | _BV (WGM00);
   TCCR0B = _BV (WGM02) | _BV (CS00); // No clock prescaler clk/1
   PWM_PIEZO = PIEZO_DIV; // buzzer freq

   // Timer2: PWM for 2 LEDs
   // Fast PWM, non-inverting on OC2A and OC2B for driving LEDs
   TCCR2A = _BV (COM2A1) | _BV (COM2B1) | _BV (WGM21) | _BV (WGM20);
   TCCR2B = _BV (CS20); // No clock prescaler clk/1

   pwm_set_leds (10, 50, 1);
}

void
pwm_set_leds (uint8_t red, uint8_t green, uint8_t blue)
{
   PWM_LED_R = red;
   PWM_LED_G = green / (256/PIEZO_DIV);
   PWM_LED_B = blue;
}
