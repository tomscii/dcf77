#pragma once

#include "sim_irq.h"

enum
{
   IRQ_DHT22_POWER = 0,
   IRQ_DHT22_DATA,
   IRQ_DHT22_COUNT
};

enum dht22_state_t
{
   OFF = 0,
   WARMUP,
   READY,
   HOST_START_LOW,
   HOST_START_HIGH,
   TRANSMIT_START_LOW,
   TRANSMIT_START_HIGH,
   TRANSMIT_BIT_LOW,
   TRANSMIT_BIT_HIGH,
   TRANSMIT_END
};

typedef struct dht22_sensor_t
{
   avr_irq_t* irq;
   avr_irq_t* irq_avr_dht22_data; // MCU pin of DATA to drive
   struct avr_t* avr;

   uint8_t power: 1;
   uint8_t data: 1;
   enum dht22_state_t state;
   int state_cycles;
   uint8_t curr_bit;
   int curr_bit_duration;

   uint8_t bits [40];
} dht22_sensor_t;

void dht22_sensor_init (struct avr_t* avr, dht22_sensor_t* p,
                        avr_irq_t* irq_data);
void dht22_sensor_set (dht22_sensor_t* p, int rh, int temp);
