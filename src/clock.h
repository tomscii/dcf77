#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

// Number of ticks (jiffies) per second
#define F_TICK 50

struct clock_t
{
   uint8_t jiffies;
   uint8_t seconds;
   uint8_t minutes;
   uint8_t hours;
};
extern struct clock_t clock;

// Bits set inside ISRs, watched and reset in main loop
struct clock_flags_t
{
   uint8_t tick: 1;
   uint8_t ovf_jiffies: 1;
};
extern volatile struct clock_flags_t clock_flags;

void clock_setup ();
void clock_adjust (int16_t ppm);
void clock_adjust_reset ();

void clock_tick ();

void clock_set_hm (uint8_t hours, uint8_t minutes);
void clock_set_hms (uint8_t hours, uint8_t minutes, uint8_t seconds);

// Drift the clock phase by the given number of jiffies
// (zero = no change to jiffy phase). This is done by letting
// time pass for the given number of jiffies without counting.
void clock_drift_phase (uint8_t jiffies);

void clock_plus_jiffy ();
void clock_minus_jiffy ();
