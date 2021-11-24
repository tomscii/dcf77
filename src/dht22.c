#include "common.h"
#include "dht22.h"
#include "utils.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#include <util/delay.h>
#include <string.h>

#define DHT22_POWER_PORT PORTD
#define DHT22_POWER_DDR  DDRD
#define DHT22_POWER      PD4

#define DHT22_DATA_PORT  PORTD
#define DHT22_DATA_DDR   DDRD
#define DHT22_DATA_PIN   PIND
#define DHT22_DATA       PD2

// Measuring period in seconds, when shown on display and when hidden
#define DHT22_MEASURE_PERIOD_FOREGROUND     15
#define DHT22_MEASURE_PERIOD_BACKGROUND    300
// Timeout for powering off the sensor
#define DHT22_POWEROFF_TIMEOUT              16

#define N_RAW_READS 360
uint8_t port_raw [N_RAW_READS];

enum dht22_state_t
{
   POWEROFF, WARMUP, IDLE, MEASURE, POSTPROC, RESULT
};
static enum dht22_state_t state = POWEROFF;
static int state_seconds = 0;  // seconds since state transition
uint8_t scheduled = 0;
static int measure_interval = DHT22_MEASURE_PERIOD_BACKGROUND;

struct dht22_status_t dht22_status;

// POSTPROC state
uint16_t pos = 0;
uint8_t bits = 0;
uint8_t bytes = 0;
uint8_t readout [5];

void dht22_trigger ();
void dht22_read ();
void dht22_postproc ();
void dht22_result ();

void dht22_setup ()
{
   DHT22_POWER_DDR |= _BV (DHT22_POWER);
   dht22_power_set (1);
   dht22_blank ();
   state = WARMUP;
   state_seconds = 0;
   scheduled = 1;
}

void dht22_blank ()
{
   memset (&dht22_status, 0, sizeof (struct dht22_status_t));
   dht22_status.blanked = 1;
}

void dht22_power_set (char on)
{
   if (on)
   {
      DHT22_POWER_PORT |= _BV (DHT22_POWER);
      // switch DATA pin to input with pull-up:
      DHT22_DATA_DDR &= ~_BV (DHT22_DATA);
      DHT22_DATA_PORT |= _BV (DHT22_DATA);
   }
   else
   {
      DHT22_POWER_PORT &= ~_BV (DHT22_POWER);
      // drive DATA low to avoid bleeding current through pull-up:
      DHT22_DATA_DDR |= _BV (DHT22_DATA);
      DHT22_DATA_PORT &= ~_BV (DHT22_DATA);
   }
}

void dht22_measure_interval (char foreground)
{
   measure_interval = foreground ? DHT22_MEASURE_PERIOD_FOREGROUND
                                 : DHT22_MEASURE_PERIOD_BACKGROUND;
}

void dht22_schedule ()
{
   if (state == POWEROFF)
   {
      dht22_power_set (1);
      state = WARMUP;
      state_seconds = 0;
   }

   if (state < MEASURE)
      scheduled = 1;
}

void dht22_on_tick ()
{
   switch (state)
   {
   case MEASURE:
      dht22_trigger ();
      _delay_ms (1);
      dht22_read ();
      break;

   case POSTPROC:
      dht22_postproc ();
      break;

   case RESULT:
      dht22_result ();
      break;

   default:
      break;
   }
}

void dht22_on_second ()
{
   switch (state)
   {
   case POWEROFF:
      if (++state_seconds > measure_interval)
         dht22_schedule ();

      break;

   case WARMUP:
      if (++state_seconds == 2)
      {
         state = scheduled ? MEASURE : IDLE;
         scheduled = 0;
         state_seconds = 0;
      }
      break;

   case IDLE:
      if (scheduled)
      {
         scheduled = 0;
         state = MEASURE;
         state_seconds = 0;
      }
      else if (++state_seconds > measure_interval)
      {
         dht22_schedule ();
      }
      else if (state_seconds >= DHT22_POWEROFF_TIMEOUT)
      {
         dht22_power_set (0);
         state = POWEROFF;
         // keep state_seconds running
      }

      break;

   default:
      break;
   }
}

/* Given that a single port read + store takes 3 clocks (12 us) and
 * the sensor provides impulse timings in the 26us - 80us range, we
 * have no choice but to read full port bytes straight without loops
 * or other logic. This is terribly wasteful in terms of storage, but
 * any control structure or bit manipulation would make us slower than
 * the minimum workable sampling rate, or cause drop-outs.
 *
 * Each input bit will take several reads, with a number of lows
 * followed by highs. Because of our low sampling rate, the exact
 * numbers will fluctuate - lows will be ~4 reads, short highs 1-3
 * reads, long highs 4-6 reads. The duration of the high half-cycle
 * determines the bit value, longs signify binary ones.
 *
 * There are 5 * 8 = 40 bits to read, after a 2*80us start cycle.
 * Therefore, 360 reads should allow us a safe margin (15 + 8 * 40 = 335).
 */
#define READ_1                                  \
   port_raw [k++] = DHT22_DATA_PIN

#define READ_10                                \
   READ_1;                                     \
   READ_1;                                     \
   READ_1;                                     \
   READ_1;                                     \
   READ_1;                                     \
   READ_1;                                     \
   READ_1;                                     \
   READ_1;                                     \
   READ_1;                                     \
   READ_1

#define READ_100                                \
   READ_10;                                     \
   READ_10;                                     \
   READ_10;                                     \
   READ_10;                                     \
   READ_10;                                     \
   READ_10;                                     \
   READ_10;                                     \
   READ_10;                                     \
   READ_10;                                     \
   READ_10

#define RAW(k)                                  \
   (port_raw [(k)] & _BV (DHT22_DATA))

void dht22_trigger ()
{
   DHT22_DATA_DDR |= _BV (DHT22_DATA);   // switch DATA to output
   DHT22_DATA_PORT &= ~_BV (DHT22_DATA); // and drive it low
}

void dht22_read ()
{
   DHT22_DATA_PORT |= _BV (DHT22_DATA);  // drive DATA high
   DHT22_DATA_DDR &= ~_BV (DHT22_DATA);  // and switch to input w/pull-up

   uint16_t k = 0;
   cli ();

   READ_100;
   READ_100;
   READ_100;

   READ_10;
   READ_10;
   READ_10;
   READ_10;
   READ_10;
   READ_10;

   sei ();

   k = 0;
   bits = 0;
   bytes = 0;

   // move past any samples before the sensor's initial pull-down
   while (RAW (k) && k < N_RAW_READS - 1)
      ++k;
   // move past the start cycle
   while (! RAW (k) && k < N_RAW_READS - 1)
      ++k;
   while (RAW (k) && k < N_RAW_READS - 1)
      ++k;

   state = POSTPROC;
   pos = k;
}

void dht22_postproc ()
{
   const uint8_t batch = 16;
   uint8_t b = 0;
   uint16_t k = pos;
   while (bits < 40 && k < N_RAW_READS - 1 && ++b < batch)
   {
      // move past the pull-down half-cycle
      while (! RAW (k) && k < N_RAW_READS - 1)
         ++k;
      uint16_t k1 = k;
      while (RAW (k) && k < N_RAW_READS - 1)
         ++k;

      readout [bytes] <<= 1;
      if (k > k1 + 3)
         readout [bytes] |= 1;

      if ((bits % 8) == 7)
      {
         ++bytes;
      }

      ++bits;

      pos = k;
   }

   if (b < batch)
   {
      state = RESULT;

   #if SIMAVR
      // To force desired readout value:
      // copy bytes from simulator output after "dht22: data = "
     #if 1
      // 35.1'C  RH 65.2%
      readout [0] = 0x02;
      readout [1] = 0x8c;
      readout [2] = 0x01;
      readout [3] = 0x5f;
      readout [4] = 0xee;
     #endif
     #if 0
      // 23.7'C  RH 35.6%
      readout [0] = 0x01;
      readout [1] = 0x64;
      readout [2] = 0x00;
      readout [3] = 0xed;
      readout [4] = 0x52;
     #endif
     #if 0
      // 4.5'C  RH 78.9%
      readout [0] = 0x03;
      readout [1] = 0x15;
      readout [2] = 0x00;
      readout [3] = 0x2d;
      readout [4] = 0x45;
     #endif
     #if 0
      // -7.2'C  RH 6.3%
      readout [0] = 0x00;
      readout [1] = 0x3f;
      readout [2] = 0x80;
      readout [3] = 0x48;
      readout [4] = 0x07;
     #endif
     #if 0
      // -15.9'C  RH 100.0%
      readout [0] = 0x03;
      readout [1] = 0xe8;
      readout [2] = 0x80;
      readout [3] = 0x9f;
      readout [4] = 0x0a;
     #endif
   #endif
   }
}

void dht22_result ()
{
   uint8_t checksum = readout [0] + readout [1] + readout [2] + readout [3];
   if (checksum == readout [4])
   {
      uint16_t rh = (readout [0] << 8) | readout [1];
      put_str ("RH ");
      put_uint (rh / 10);
      putchar ('.');
      put_uint (rh % 10);
      put_str ("%  T ");
      uint16_t traw = (readout [2] << 8) | readout [3];
      int16_t temp = traw & 0x7fff;
      if (traw & 0x8000)
      {
         temp *= -1;
         putchar ('-');
         traw &= 0x7fff;
      }
      put_uint (traw / 10);
      putchar ('.');
      put_uint (traw % 10);
      put_str ("'C\r\n");

      dht22_status.rh_last = rh & 0x7fff;
      dht22_status.temp_last = temp;
      if (dht22_status.blanked)
      {
         dht22_status.blanked = 0;
         dht22_status.rh_min = dht22_status.rh_last;
         dht22_status.rh_max = dht22_status.rh_last;
         dht22_status.temp_min = dht22_status.temp_last;
         dht22_status.temp_max = dht22_status.temp_last;
      }
      else
      {
         if (dht22_status.rh_min > dht22_status.rh_last)
            dht22_status.rh_min = dht22_status.rh_last;
         if (dht22_status.rh_max < dht22_status.rh_last)
            dht22_status.rh_max = dht22_status.rh_last;
         if (dht22_status.temp_min > dht22_status.temp_last)
            dht22_status.temp_min = dht22_status.temp_last;
         if (dht22_status.temp_max < dht22_status.temp_last)
            dht22_status.temp_max = dht22_status.temp_last;
      }
      dht22_status.updated = 1;
   }
   else
      put_str ("checksum error\r\n");

   state = IDLE;
   state_seconds = 0;
}
