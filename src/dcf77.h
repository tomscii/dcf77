#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

void dcf77_setup ();

void dcf77_power_set (char on);

void dcf77_on_tick ();
void dcf77_on_second ();
