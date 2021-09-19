#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

#define RX_BUFSIZE 80

int usart_getchar (FILE* stream);
int usart_putchar (char c, FILE* stream);
