#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

struct dht22_status_t
{
   int16_t temp_last;
   int16_t temp_min;
   int16_t temp_max;
   int16_t rh_last;
   int16_t rh_min;
   int16_t rh_max;

   uint8_t blanked: 1;  // set if no min/max is valid
   uint8_t updated: 1;  // set on updates, zeroed by readout (display)
};
extern struct dht22_status_t dht22_status;

void dht22_setup ();
void dht22_blank ();

void dht22_power_set (char on);
void dht22_measure_interval (char foreground);

void dht22_schedule ();

void dht22_on_tick ();
void dht22_on_second ();
