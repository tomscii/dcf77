#include "common.h"
#include "clock.h"
#include "dcf77.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#include <avr_mcu_section.h>
AVR_MCU (F_CPU, "atmega328p");
AVR_MCU_VOLTAGES (3300, 3300, 3300);

extern struct clock_t clock;

extern uint8_t has_been_synced;
extern enum dcf77_state_t state;

AVR_MCU_VCD_FILE ("trace.vcd", 1000);

AVR_MCU_VCD_IRQ (TIMER1_OVF);
AVR_MCU_VCD_IRQ (USART_UDRE);
AVR_MCU_VCD_IRQ (USART_RX);

//AVR_MCU_SIMAVR_CONSOLE (&GPIOR0);

const struct avr_mmcu_vcd_trace_t _mytrace[]  _MMCU_ = {
   { AVR_MCU_VCD_SYMBOL ("OCR1AH"), .what = (void*)&OCR1AH, },
   { AVR_MCU_VCD_SYMBOL ("OCR1AL"), .what = (void*)&OCR1AL, },
   { AVR_MCU_VCD_SYMBOL ("UDR0"), .what = (void*)&UDR0, },
   { AVR_MCU_VCD_SYMBOL ("UDRE0"), .mask = (1 << UDRE0), .what = (void*)&UCSR0A, },
   { AVR_MCU_VCD_SYMBOL ("LCD_CLK"), .mask = (1 << 3), .what = (void*)&PORTC, },
   { AVR_MCU_VCD_SYMBOL ("LCD_COM"), .mask = (1 << 4), .what = (void*)&PORTC, },
   { AVR_MCU_VCD_SYMBOL ("LCD_OE/"), .mask = (1 << 5), .what = (void*)&PORTC, },
   { AVR_MCU_VCD_SYMBOL ("LCD_DATA"), .mask = 7, .what = (void*)&PORTC, },
   { AVR_MCU_VCD_SYMBOL ("DCF77_POWER"), .mask = (1 << 7), .what = (void*)&PORTD, },
   { AVR_MCU_VCD_SYMBOL ("DHT22_POWER"), .mask = (1 << 4), .what = (void*)&PORTD, },
};
