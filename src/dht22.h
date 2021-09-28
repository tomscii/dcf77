#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

void dht22_setup ();

void dht22_power_set (char on);

void dht22_trigger ();
void dht22_read ();
