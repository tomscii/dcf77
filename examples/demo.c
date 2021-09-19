/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <joerg@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.        Joerg Wunsch
 * ----------------------------------------------------------------------------
 *
 * Simple AVR demonstration.  Controls a LED that can be directly
 * connected from OC1/OC1A to GND.  The brightness of the LED is
 * controlled with the PWM.  After each period of the PWM, the PWM
 * value is either incremented or decremented, that's all.
 *
 * $Id$
 */

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>

enum { UP, DOWN };

ISR (TIMER1_OVF_vect)		/* Note [2] */
{
    static uint16_t pwm;	/* Note [3] */
    static uint8_t direction;

    switch (direction)		/* Note [4] */
    {
        case UP:
            if (++pwm == 1023)
            {
                direction = DOWN;
                PORTC ^= _BV (PC5);
            }
            break;

        case DOWN:
            if (--pwm == 0)
            {
                direction = UP;
                PORTC ^= _BV (PC5);
            }
            break;
    }

    OCR1A = pwm;		/* Note [5] */
}

void
ioinit (void)			/* Note [6] */
{
#if 0
    // set all IO pins as output, driven low
    PORTB = 0;
    DDRB = 0xff;
    PORTC = 0;
    DDRC = 0xff;
    PORTD = 0;
    DDRD = 0xff;
#else
    // set all IO pins as input, pull-up active
    PORTB = 0xff;
    DDRB = 0;
    PORTC = 0xff;
    DDRC = 0;
    PORTD = 0xff;
    DDRD = 0;
#endif

    ADCSRA &= ~_BV(ADEN);       // disable ADC
    ACSR |= _BV(ACD);           // disable Analog Comparator
    SPCR &= ~_BV(SPE);          // disable SPI
    TWCR &= ~_BV(TWEN);         // disable TWI

    power_all_disable();
    power_timer1_enable();

    /* Timer 1 is 10-bit PWM */
    TCCR1A = _BV(WGM10) | _BV(WGM11) | _BV(COM1A1);

    // Start timer 1.
    TCCR1B |= _BV(CS10);

    /* Set PWM value to 0. */
    OCR1A = 0;

    /* Enable OC1 as output. */
    DDRB |= _BV (PB1);

    // Enable LED pin as output
    DDRC |= _BV (PC5);

    /* Enable timer 1 overflow interrupt. */
    TIMSK1 = _BV (TOIE1);
    sei ();
}

int
main (void)
{
    ioinit ();

    /* loop forever, the interrupts are doing the rest */

    for (;;)			/* Note [7] */
    {
        sleep_mode();
    }

    return (0);
}
