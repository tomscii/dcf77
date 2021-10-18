#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

// Bits set inside ISRs, watched and reset in main loop
struct dcf77_flags_t
{
   uint8_t curr_level: 1;
};
extern volatile struct dcf77_flags_t dcf77_flags;

void dcf77_setup ();

void dcf77_power_set (char on);

void dcf77_on_tick ();
void dcf77_on_second ();
