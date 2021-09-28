// CPU core frequency
#define F_CPU 250000UL


#define DEBUG_LED_TOGGLE \
PORTB ^= _BV (PB1)

#define DEBUG_LED_ON \
PORTB |= _BV (PB1)

#define DEBUG_LED_OFF \
PORTB &= ~_BV (PB1)

#define DEBUG_LED_ENABLE_OUTPUT \
DDRB |= _BV (PB1)
