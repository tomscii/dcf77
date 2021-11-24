#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <simavr/sim_avr.h>
#include <simavr/sim_time.h>

#include "dht22_sensor.h"

#define WARMUP_DELAY_US            1000000
#define TRANSMIT_START_DELAY_US         50
#define TRANSMIT_START_LOW_US           80
#define TRANSMIT_START_HIGH_US          80
#define TRANSMIT_BIT_LOW_US             50
#define TRANSMIT_BIT_HIGH_0_US          28
#define TRANSMIT_BIT_HIGH_1_US          70
#define TRANSMIT_END_LOW_US             80

static avr_cycle_count_t
dht22_ready (struct avr_t* avr, avr_cycle_count_t when, void* param);
static avr_cycle_count_t
dht22_fsm (struct avr_t* avr, avr_cycle_count_t when, void* param);

static avr_cycle_count_t
dht22_ready (struct avr_t* avr, avr_cycle_count_t when, void* param)
{
   dht22_sensor_t* p = (dht22_sensor_t *)param;

   if (p->state == WARMUP)
   {
      // startup delay has elapsed, sensor is now able to respond
      p->state = READY;
      printf ("dht22: READY\n");
   }

   return 0;
}

static avr_cycle_count_t
dht22_fsm (struct avr_t* avr, avr_cycle_count_t when, void* param)
{
   dht22_sensor_t* p = (dht22_sensor_t *)param;

   ++p->state_cycles;

   //printf ("dht22: state=%d state_cycles=%d\n", p->state, p->state_cycles);
   uint32_t state_usecs = avr_cycles_to_usec (avr, p->state_cycles);

   switch (p->state)
   {
   case HOST_START_HIGH:
      if (state_usecs < TRANSMIT_START_DELAY_US)
         break;

      // our turn to pull down the data line
      p->data = 0;
      avr_raise_irq (p->irq_avr_dht22_data, p->data);

      p->state = TRANSMIT_START_LOW;
      p->state_cycles = 0;
      break;

   case TRANSMIT_START_LOW:
      if (state_usecs < TRANSMIT_START_LOW_US)
         break;

      // pull up
      p->data = 1;
      avr_raise_irq (p->irq_avr_dht22_data, p->data);

      p->state = TRANSMIT_START_HIGH;
      p->state_cycles = 0;
      break;

   case TRANSMIT_START_HIGH:
      if (state_usecs < TRANSMIT_START_HIGH_US)
         break;

      // falling edge of first data bit
      p->data = 0;
      avr_raise_irq (p->irq_avr_dht22_data, p->data);

      p->curr_bit = 0;
      p->state = TRANSMIT_BIT_LOW;
      p->state_cycles = 0;
      break;

   case TRANSMIT_BIT_LOW:
      if (state_usecs < TRANSMIT_BIT_LOW_US)
         break;

      // rising edge of data bit
      p->data = 1;
      avr_raise_irq (p->irq_avr_dht22_data, p->data);

      p->curr_bit_duration =
         p->bits [p->curr_bit] ? TRANSMIT_BIT_HIGH_1_US
                               : TRANSMIT_BIT_HIGH_0_US;

      p->state = TRANSMIT_BIT_HIGH;
      p->state_cycles = 0;
      break;

   case TRANSMIT_BIT_HIGH:
      if (state_usecs < p->curr_bit_duration)
         break;

      // falling edge of next data bit, or end bit
      p->data = 0;
      avr_raise_irq (p->irq_avr_dht22_data, p->data);

      p->state = (++p->curr_bit == 40) ? TRANSMIT_END
                                       : TRANSMIT_BIT_LOW;
      p->state_cycles = 0;

      break;

   case TRANSMIT_END:
      if (state_usecs < TRANSMIT_END_LOW_US)
         break;

      // release line
      p->data = 1;
      avr_raise_irq (p->irq_avr_dht22_data, p->data);

      p->state = READY;
      printf ("dht22: READY\n");
      return 0; // no more callbacks to dht22_fsm ()
   }

   return when + 1;
}

static void
dht22_data_hook (struct avr_irq_t* irq, uint32_t value, void* param)
{
   dht22_sensor_t* p = (dht22_sensor_t *)param;

   if (p->state == READY && p->data && !value)
   { // initial falling edge by host
      p->state = HOST_START_LOW;
      p->data = value;
   }
   else if (p->state == HOST_START_LOW && !p->data && value)
   {
      p->state = HOST_START_HIGH;
      p->data = value;
      p->state_cycles = 0;
      avr_cycle_timer_register(p->avr, 1, dht22_fsm, p);
   }
}

static void
dht22_power_hook (struct avr_irq_t* irq, uint32_t value, void* param)
{
   dht22_sensor_t* p = (dht22_sensor_t *)param;

   if (!p->power && value)
   {
      p->data = 1;
      avr_raise_irq (p->irq_avr_dht22_data, p->data);
      avr_cycle_timer_register_usec(p->avr, WARMUP_DELAY_US, dht22_ready, p);
      p->state = WARMUP;
      printf ("dht22: WARMUP\n");
   }
   else if (p->power && !value)
   {
      p->state = OFF;
      printf ("dht22: OFF\n");
   }
   p->power = value;
}

static const char* irq_names [IRQ_DHT22_COUNT] =
{
   [IRQ_DHT22_POWER] = "dht22.power",
   [IRQ_DHT22_DATA] = "dht22.data",
};

void
dht22_sensor_init (struct avr_t* avr, dht22_sensor_t* p, avr_irq_t* irq_data)
{
   memset (p, 0, sizeof (*p));

   p->avr = avr;
   p->irq = avr_alloc_irq (&avr->irq_pool, 0, IRQ_DHT22_COUNT, irq_names);
   p->irq_avr_dht22_data = irq_data;

   avr_irq_register_notify(p->irq + IRQ_DHT22_POWER, dht22_power_hook, p);
   avr_irq_register_notify(p->irq + IRQ_DHT22_DATA, dht22_data_hook, p);
}

void
dht22_sensor_set (dht22_sensor_t* p, int rh, int temp)
{
   uint8_t rh_hi = rh >> 8;
   uint8_t rh_lo = rh & 0xff;
   uint8_t temp_hi, temp_lo;
   if (temp < 0)
   {
      temp = -temp;
      temp_hi = temp >> 8;
      temp_hi |= 0x80;
      temp_lo = temp & 0xff;
   }
   else
   {
      temp_hi = temp >> 8;
      temp_lo = temp & 0xff;
   }
   uint8_t checksum = rh_hi + rh_lo + temp_hi + temp_lo;
   printf ("dht22: data = %02x %02x %02x %02x %02x\n",
           rh_hi, rh_lo, temp_hi, temp_lo, checksum);
   int k = 0;

   p->bits [k++] = (rh_hi & 0x80) != 0;
   p->bits [k++] = (rh_hi & 0x40) != 0;
   p->bits [k++] = (rh_hi & 0x20) != 0;
   p->bits [k++] = (rh_hi & 0x10) != 0;
   p->bits [k++] = (rh_hi & 0x08) != 0;
   p->bits [k++] = (rh_hi & 0x04) != 0;
   p->bits [k++] = (rh_hi & 0x02) != 0;
   p->bits [k++] = (rh_hi & 0x01) != 0;

   p->bits [k++] = (rh_lo & 0x80) != 0;
   p->bits [k++] = (rh_lo & 0x40) != 0;
   p->bits [k++] = (rh_lo & 0x20) != 0;
   p->bits [k++] = (rh_lo & 0x10) != 0;
   p->bits [k++] = (rh_lo & 0x08) != 0;
   p->bits [k++] = (rh_lo & 0x04) != 0;
   p->bits [k++] = (rh_lo & 0x02) != 0;
   p->bits [k++] = (rh_lo & 0x01) != 0;

   p->bits [k++] = (temp_hi & 0x80) != 0;
   p->bits [k++] = (temp_hi & 0x40) != 0;
   p->bits [k++] = (temp_hi & 0x20) != 0;
   p->bits [k++] = (temp_hi & 0x10) != 0;
   p->bits [k++] = (temp_hi & 0x08) != 0;
   p->bits [k++] = (temp_hi & 0x04) != 0;
   p->bits [k++] = (temp_hi & 0x02) != 0;
   p->bits [k++] = (temp_hi & 0x01) != 0;

   p->bits [k++] = (temp_lo & 0x80) != 0;
   p->bits [k++] = (temp_lo & 0x40) != 0;
   p->bits [k++] = (temp_lo & 0x20) != 0;
   p->bits [k++] = (temp_lo & 0x10) != 0;
   p->bits [k++] = (temp_lo & 0x08) != 0;
   p->bits [k++] = (temp_lo & 0x04) != 0;
   p->bits [k++] = (temp_lo & 0x02) != 0;
   p->bits [k++] = (temp_lo & 0x01) != 0;

   p->bits [k++] = (checksum & 0x80) != 0;
   p->bits [k++] = (checksum & 0x40) != 0;
   p->bits [k++] = (checksum & 0x20) != 0;
   p->bits [k++] = (checksum & 0x10) != 0;
   p->bits [k++] = (checksum & 0x08) != 0;
   p->bits [k++] = (checksum & 0x04) != 0;
   p->bits [k++] = (checksum & 0x02) != 0;
   p->bits [k++] = (checksum & 0x01) != 0;

   printf ("dht22: bits =");
   for (k = 0; k < 40; ++k)
   {
      if (k % 8 == 0)
         printf (" ");
      printf ("%d", p->bits [k]);
   }
   printf ("\n");
}
