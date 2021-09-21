#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

void pwm_setup ();

void pwm_set_leds (uint8_t red, uint8_t green, uint8_t blue);
