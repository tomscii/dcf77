/* Toggle LED on PC5 every time a 'b' is received on the USART, busy looping */
#ifndef F_CPU
#define F_CPU 40000UL
#endif

#define FOSC F_CPU
#define BAUD 1200
#define MYUBRR FOSC/16/BAUD-1

#include <avr/io.h>
#include <util/delay.h>

void USART_Init(unsigned int ubrr) {
   UBRR0H = (unsigned char)(ubrr>>8);
   UBRR0L = (unsigned char)ubrr;
   UCSR0B = (1<<RXEN0)|(1<<TXEN0);
   UCSR0C = (1<<USBS0)|(3<<UCSZ00);
}

void USART_Transmit( unsigned char data ){
   while ( !( UCSR0A & (1<<UDRE0)) );
   UDR0 = data;
}

unsigned char USART_Receive( void ){
   while ( !(UCSR0A & (1<<RXC0)) );
   return UDR0;
}

int main(void) {

	USART_Init(MYUBRR);

	DDRC |= (1 << PC5);

	while(1) {
		char c = USART_Receive();
		if (c == 'b') {
			PORTC ^= (1 << PC5);
		}
	}
	return 0;
}
