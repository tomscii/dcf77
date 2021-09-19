#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

int
main (void)
{
    PORTB = 0;
    DDRB = 0xff;
    PORTC = 0;
    DDRC = 0xff;
    PORTD = 0;
    DDRD = 0xff;

    //DDRC |= _BV (PC5);
    //PORTC |= _BV (PC5);
    for (;;)
    {
        set_sleep_mode(SM1 | SM0);
        cli();
        sleep_enable();
        sei();
        sleep_cpu();
        sleep_disable();
        sei();
        PORTC |= _BV (PC5);
        //PORTC ^= _BV (PC5);
    }

    return (0);
}
