#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

void lcd_setup ();

void lcd_send_data ();

#define LCD_PORT PORTC
#define LCD_DDR  DDRC

#define LCD_D0   PC0
#define LCD_D1   PC1
#define LCD_D2   PC2
#define LCD_CLK  PC3
#define LCD_COM  PC4
#define LCD_OE_  PC5

// disable shift register output, macro to "inline" into ISR
#define LCD_OUTPUT_DISABLE                      \
   LCD_PORT |= _BV (LCD_OE_)
