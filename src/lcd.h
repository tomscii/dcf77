#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

void lcd_setup ();

// call this periodically to refresh LCD
void lcd_send_frame ();

// idx: 1..6 from left to right
void lcd_set_digit (uint8_t idx, char c);
// idx: 1..5 from left to right
void lcd_set_dot (uint8_t idx, char on);
void lcd_set_colons (char on);

// immediate: does not require lcd_commit
void lcd_set_dot5_immediate (char c);

// apply changes from lcd_set_* to display buffer
void lcd_commit ();
