#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

// Number of ticks (jiffies) per second
#define F_TICK 50

struct clock_t
{
   char jiffies; // N.B.: signed so we can simply drift it backwards
   char seconds;
   char minutes;
   char hours;

   char day;   // day of month, 1..31
   char month; // month, 1..12
   char year;  // mod 100
   char dow;   // day of week, 1..7, 1=Monday
   char dst;   // 1 if DST is in effect (CEST), 0 if not (CET)
};
extern struct clock_t clock;

// Bits set inside ISRs, watched and reset in main loop
struct clock_flags_t
{
   uint8_t tick: 1;
   uint8_t ovf_jiffies: 1;
   uint8_t ovf_seconds: 1;
   uint8_t ovf_minutes: 1;
   uint8_t ovf_hours: 1;
   uint8_t ovf_days: 1;
   uint8_t ovf_months: 1;
};
extern volatile struct clock_flags_t clock_flags;

void clock_setup ();
void clock_adjust (int16_t ppm);
void clock_adjust_reset ();

void clock_tick ();

void clock_set (const struct clock_t* ref);
void clock_set_hms (char hours, char minutes, char seconds);

// Drift the clock phase backwards by the given number of jiffies
void clock_drift_phase (char jiffies);

void clock_plus_jiffy ();
void clock_minus_jiffy ();
