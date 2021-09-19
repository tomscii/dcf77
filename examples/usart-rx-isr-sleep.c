/* USART receive bytes via ISR */

#ifndef F_CPU
#define F_CPU 40000UL
#endif

#define FOSC F_CPU
#define BAUD 1200
#define MYUBRR FOSC/16/BAUD-1

#include <avr/io.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>

void USART_Init(unsigned int ubrr) {
   UBRR0H = (unsigned char)(ubrr>>8);
   UBRR0L = (unsigned char)ubrr;
   UCSR0B = (1<<RXEN0);
   UCSR0C = (1<<USBS0)|(3<<UCSZ00);
}

void USART_RX_int() {
	UCSR0B |= (1 << RXCIE0);
	sei();
}

unsigned char USART_Receive( void ){
   while ( !(UCSR0A & (1<<RXC0)) );
   return UDR0;
}

ISR(USART_RX_vect) {
	char c = USART_Receive();
	if (c == 'b') {
		PORTC ^= (1 << PC5);
	}
}

int main(void) {

	power_all_disable();
	power_usart0_enable();

	USART_Init(MYUBRR);
	USART_RX_int();

	DDRC |= (1 << PC5);

	while (1) {
		set_sleep_mode(0);
		sleep_mode();
	}
	return 0;
}
