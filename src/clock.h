#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

struct clock_t
{
   uint8_t jiffies;
   uint8_t seconds;
   uint8_t minutes;
   uint8_t hours;

   // used to set and fine-tune phase:
   uint8_t phase_jiffy;
   int16_t phase_vernier;
};
extern struct clock_t clock;

// Bits set inside ISRs, watched and reset in main loop
struct clock_flags_t
{
   uint8_t tick: 1;
   uint8_t ovf_jiffies: 1;
   uint8_t skip_jiffy: 1;
};
extern volatile struct clock_flags_t clock_flags;

void clock_setup ();
void clock_adjust (int16_t ppm);

void clock_tick ();

void clock_set_hm (uint8_t hours, uint8_t minutes);
void clock_set_hms (uint8_t hours, uint8_t minutes, uint8_t seconds);

// Set the clock phase to the point defined by the given jiffy
// (zero = no change to jiffy phase).
// Use vernier to fine-tune phase (sub-jiffy), either direction.
void clock_set_phase (uint8_t jiffy, int16_t vernier);

void clock_plus_jiffy ();
void clock_minus_jiffy ();
