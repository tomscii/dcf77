#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

void usart_setup ();

int usart_getchar (FILE* stream);

int usart_putchar (char c, FILE* stream);
