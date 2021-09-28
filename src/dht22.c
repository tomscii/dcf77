#include "common.h"
#include "dht22.h"
#include "utils.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#define DHT22_POWER_PORT PORTD
#define DHT22_POWER_DDR  DDRD
#define DHT22_POWER      PD4

#define DHT22_DATA_PORT  PORTD
#define DHT22_DATA_DDR   DDRD
#define DHT22_DATA_PIN   PIND
#define DHT22_DATA       PD2

#define N_RAW_READS 360
uint8_t port_raw [N_RAW_READS];

void dht22_setup ()
{
   DHT22_POWER_PORT |= _BV (DHT22_POWER);
   dht22_power_set (1);
}

void dht22_power_set (char on)
{
   if (on)
      DHT22_POWER_DDR |= _BV (DHT22_POWER);
   else
      DHT22_POWER_DDR &= ~_BV (DHT22_POWER);
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
#define READ_10                                                 \
   port_raw [k++] = DHT22_DATA_PIN;                             \
   port_raw [k++] = DHT22_DATA_PIN;                             \
   port_raw [k++] = DHT22_DATA_PIN;                             \
   port_raw [k++] = DHT22_DATA_PIN;                             \
   port_raw [k++] = DHT22_DATA_PIN;                             \
   port_raw [k++] = DHT22_DATA_PIN;                             \
   port_raw [k++] = DHT22_DATA_PIN;                             \
   port_raw [k++] = DHT22_DATA_PIN;                             \
   port_raw [k++] = DHT22_DATA_PIN;                             \
   port_raw [k++] = DHT22_DATA_PIN

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

   // move past any samples before the sensor's initial pull-down
   while (RAW (k) && k < N_RAW_READS - 1)
      ++k;
   // move past the start cycle
   while (! RAW (k) && k < N_RAW_READS - 1)
      ++k;
   while (RAW (k) && k < N_RAW_READS - 1)
      ++k;

   uint8_t bits = 0;
   uint8_t bytes = 0;
   uint8_t readout [5];
   while (bits < 40 && k < N_RAW_READS - 1)
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
   }
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
      //int16_t temp = traw & 0x7fff;
      if (traw & 0x8000)
      {
         //temp *= -1;
         putchar ('-');
      }
      put_uint (traw / 10);
      putchar ('.');
      put_uint (traw % 10);
      put_str ("'C\r\n");
   }
   else
      put_str ("checksum error\r\n");
}
