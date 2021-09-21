// CPU core frequency
#define F_CPU 125000UL

// UART baud rate
#define UART_BAUD  1200UL


#define DEBUG_LED_TOGGLE \
PORTB ^= _BV (PB1)

#define DEBUG_LED_ON \
PORTB |= _BV (PB1)

#define DEBUG_LED_OFF \
PORTB &= ~_BV (PB1)

#define DEBUG_LED_ENABLE_OUTPUT \
DDRB |= _BV (PB1)
