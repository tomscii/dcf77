%.o: %.c
	avr-gcc -Wall -g -Os -mmcu=atmega328p -o $@ $<

%.hex: %.o
	avr-objcopy -j .text -j .data -O ihex $< $@

prog-%: %.hex
	avrdude -c usbtiny -p atmega328p -B 10kHz -U flash:w:$< -Ulfuse:w:0xe5:m
