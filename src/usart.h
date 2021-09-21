#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

#define RX_BUFSIZE 80

extern FILE usart_str;

void usart_setup ();
int usart_getchar (FILE* stream);
int usart_putchar (char c, FILE* stream);
