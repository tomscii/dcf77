#ifndef F_CPU
#define F_CPU 40000UL
#endif

#include <avr/io.h>
#include <util/delay.h>

int main(void) {
	DDRC |= (1 << PC5);
	while (1) {
		// Toggle LED at port PC5
		PORTC ^= (1 << PC5);
		_delay_ms(100);
		PORTC ^= (1 << PC5);
		_delay_ms(900);
	}
	return 0;
}
