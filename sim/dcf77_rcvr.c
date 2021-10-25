#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <simavr/sim_avr.h>
#include <simavr/sim_time.h>

#include "dcf77_rcvr.h"

#define USECS_PER_SECOND (1000 * 1000)
#define HZ (10)

static int phase = -1; // trigger initialization from actual sub-second phase
static int bit_cnt = -1; // trigger initialization from actual seconds
static char bits [60];

static char
dcf77_calc_parity (char* begin, const char* end)
{
   int p = 0;
   for (char* c = begin; c != end; ++c)
   {
      if (*c == '1')
         ++p;
   }
   return (p % 2) ? '1' : '0';
}

static void
dcf77_encode_time ()
{
   struct timespec ts;
   clock_gettime (CLOCK_REALTIME, &ts);
   time_t time = ((ts.tv_sec + 60) / 60) * 60; // start of next minute
   //time += 86400 * 4;
   struct tm* tm = localtime (&time);

   if (bit_cnt < 0)
      bit_cnt = ts.tv_sec % 60;
   if (phase < 0)
      phase = ts.tv_nsec / (100 * 1000 * 1000);

   int year = tm->tm_year % 100;
   int mon = tm->tm_mon + 1;
   int day = tm->tm_mday;
   int hour = tm->tm_hour;
   int min = tm->tm_min;
   int dow = (tm->tm_wday == 0) ? 7 : tm->tm_wday;
   int dst = tm->tm_isdst;

   printf ("dcf77 broadcast time: %02u-%02u-%02u %02u:%02u dow:%d dst:%d\n",
           year, mon, day, hour, min, dow, dst);

   for (int k = 0; k < 20; ++k)
      bits [k] = '0';

   if (dst)
      bits [17] = '1';
   else
      bits [18] = '1';
   bits [20] = '1';
   bits [21] = ((min % 10) & 1) ? '1' : '0';
   bits [22] = ((min % 10) & 2) ? '1' : '0';
   bits [23] = ((min % 10) & 4) ? '1' : '0';
   bits [24] = ((min % 10) & 8) ? '1' : '0';
   bits [25] = ((min / 10) & 1) ? '1' : '0';
   bits [26] = ((min / 10) & 2) ? '1' : '0';
   bits [27] = ((min / 10) & 4) ? '1' : '0';
   bits [28] = dcf77_calc_parity (bits + 21, bits + 28);
   bits [29] = ((hour % 10) & 1) ? '1' : '0';
   bits [30] = ((hour % 10) & 2) ? '1' : '0';
   bits [31] = ((hour % 10) & 4) ? '1' : '0';
   bits [32] = ((hour % 10) & 8) ? '1' : '0';
   bits [33] = ((hour / 10) & 1) ? '1' : '0';
   bits [34] = ((hour / 10) & 2) ? '1' : '0';
   bits [35] = dcf77_calc_parity (bits + 29, bits + 35);
   bits [36] = ((day % 10) & 1) ? '1' : '0';
   bits [37] = ((day % 10) & 2) ? '1' : '0';
   bits [38] = ((day % 10) & 4) ? '1' : '0';
   bits [39] = ((day % 10) & 8) ? '1' : '0';
   bits [40] = ((day / 10) & 1) ? '1' : '0';
   bits [41] = ((day / 10) & 2) ? '1' : '0';
   bits [42] = ((dow % 10) & 1) ? '1' : '0';
   bits [43] = ((dow % 10) & 2) ? '1' : '0';
   bits [44] = ((dow % 10) & 4) ? '1' : '0';
   bits [45] = ((mon % 10) & 1) ? '1' : '0';
   bits [46] = ((mon % 10) & 2) ? '1' : '0';
   bits [47] = ((mon % 10) & 4) ? '1' : '0';
   bits [48] = ((mon % 10) & 8) ? '1' : '0';
   bits [49] = ((mon / 10) & 1) ? '1' : '0';
   bits [50] = ((year % 10) & 1) ? '1' : '0';
   bits [51] = ((year % 10) & 2) ? '1' : '0';
   bits [52] = ((year % 10) & 4) ? '1' : '0';
   bits [53] = ((year % 10) & 8) ? '1' : '0';
   bits [54] = ((year / 10) & 1) ? '1' : '0';
   bits [55] = ((year / 10) & 2) ? '1' : '0';
   bits [56] = ((year / 10) & 4) ? '1' : '0';
   bits [57] = ((year / 10) & 8) ? '1' : '0';
   bits [58] = dcf77_calc_parity (bits + 36, bits + 58);
   bits [59] = '_';
}

static avr_cycle_count_t
dcf77_tick (struct avr_t* avr, avr_cycle_count_t when, void* param)
{
   dcf77_rcvr_t* p = (dcf77_rcvr_t *)param;
   uint8_t data = 0;

   if (++phase == 10)
   {
      phase = 0;

      if (++bit_cnt == 60)
      {
         bit_cnt = 0;
         dcf77_encode_time ();
      }
   }

   switch (phase)
   {
   case 0:
      data = bits [bit_cnt] != '_';
      break;
   case 1:
      data = bits [bit_cnt] == '1';
      break;
   default:
      data = 0;
      break;
   }

   if (!p->power)
      data = 0;

   if (p->data != !data) // inverted signal!
   {
      p->data = !data;
      avr_raise_irq (p->irq + IRQ_DCF77_DATA, p->data);
   }

   return when + avr_usec_to_cycles (p->avr, USECS_PER_SECOND / HZ);
}

static void dcf77_power_hook (struct avr_irq_t* irq, uint32_t value, void* param)
{
   dcf77_rcvr_t* p = (dcf77_rcvr_t *)param;

   if (!p->power && value)
   {
      printf ("dcf77 receiver power on\n");

      p->data = 1;
      avr_raise_irq (p->irq + IRQ_DCF77_DATA, p->data);
      avr_cycle_timer_register_usec (p->avr, USECS_PER_SECOND / HZ, dcf77_tick, p);
   }
   else if (p->power && !value)
   {
      printf ("dcf77 receiver power off\n");
   }
   p->power = value;
}


static const char* irq_names [IRQ_DCF77_COUNT] =
{
   [IRQ_DCF77_POWER] = "dcf77.power",
   [IRQ_DCF77_DATA] = "dcf77.data",
};

void
dcf77_rcvr_init (struct avr_t* avr, dcf77_rcvr_t* p)
{
   memset (p, 0, sizeof (*p));

   p->avr = avr;
   p->irq = avr_alloc_irq (&avr->irq_pool, 0, IRQ_DCF77_COUNT, irq_names);

   avr_irq_register_notify (p->irq + IRQ_DCF77_POWER, dcf77_power_hook, p);

   dcf77_encode_time ();
}
