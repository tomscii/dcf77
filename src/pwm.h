#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

#define IDX_LED_R   0
#define IDX_LED_G   1
#define IDX_LED_B   2

void pwm_setup ();

void pwm_set_leds (uint8_t red, uint8_t green, uint8_t blue);

uint8_t pwm_get_led (uint8_t led_ix);
void pwm_set_led (uint8_t led_ix, uint8_t amount);
void pwm_nudge_led (uint8_t led_ix, int8_t amount);

void pwm_piezo_set (char on);
