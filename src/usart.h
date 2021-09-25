#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

// Bits set inside ISRs, watched and reset in main loop
struct usart_flags_t
{
   uint8_t rx: 1;
};
extern volatile struct usart_flags_t usart_flags;

void usart_setup ();

int usart_getchar (FILE* stream);

int usart_putchar (char c, FILE* stream);
