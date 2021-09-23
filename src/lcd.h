#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

void lcd_setup ();

// Pass in an array of 3 elements for a total of 48 bits.
void lcd_set_bits (uint16_t* data);

void lcd_send_frame ();
