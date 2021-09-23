#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

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
   uint8_t skip_jiffy: 1;
};
extern volatile struct clock_flags_t clock_flags;

void clock_setup ();

void clock_tick ();

void clock_set (uint8_t hours, uint8_t minutes, uint8_t seconds);

void clock_plus_jiffy ();
void clock_minus_jiffy ();
