#include <stdint.h>
#include <stdio.h>

extern const char* d2[];

void put_str (const char* str);
void put_byte_hex (uint8_t x);
void put_int (int16_t x);
void put_uint (uint16_t x);
void put_q4_11 (int16_t x);

// Return the number of 1-bits in x
uint8_t n1bits (uint16_t x);

// Return the bit-reversed value (MSB <--> LSB, etc) of x
uint16_t revbits (uint16_t x);
