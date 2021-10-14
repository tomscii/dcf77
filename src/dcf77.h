#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

// Bits set inside ISRs, watched and reset in main loop
struct dcf77_flags_t
{
   uint8_t trigger: 1;    // flag that an edge was triggered
   uint8_t curr_level: 1;
   uint8_t next_edge: 1;  // 0 = falling edge, 1 = rising edge
   uint8_t jiffies;       // value of clock.jiffies on trigger
   uint16_t counter;      // value of Timer1 on trigger
};
extern volatile struct dcf77_flags_t dcf77_flags;

void dcf77_setup ();

void dcf77_power_set (char on);

void dcf77_on_tick ();
void dcf77_on_second ();
