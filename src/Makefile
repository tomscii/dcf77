PRG            = dcf77
OBJ            = main.o usart.o
MCU_TARGET     = atmega328p
OPTIMIZE       = -Os

DEFS           =
LIBS           =

# You should not have to change anything below here.

CC             = avr-gcc

# Override is only needed by avr-lib build system.

override CFLAGS        = -g -Wall $(OPTIMIZE) -mmcu=$(MCU_TARGET) $(DEFS)
override LDFLAGS       = -Wl,-Map,$(PRG).map

OBJCOPY        = avr-objcopy
OBJDUMP        = avr-objdump

all: $(PRG).elf lst text eeprom

$(PRG).elf: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf *.o $(PRG).elf *.eps *.png *.pdf *.bak
	rm -rf *.lst *.map $(EXTRA_CLEAN_FILES)

lst:  $(PRG).lst

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

# Rules for building the .text rom images

text: hex bin srec

hex:  $(PRG).hex
bin:  $(PRG).bin
srec: $(PRG).srec

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

%.srec: %.elf
	$(OBJCOPY) -j .text -j .data -O srec $< $@

%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@

# Rules for building the .eeprom rom images

eeprom: ehex ebin esrec

ehex:  $(PRG)_eeprom.hex
ebin:  $(PRG)_eeprom.bin
esrec: $(PRG)_eeprom.srec

%_eeprom.hex: %.elf
	$(OBJCOPY) -j .eeprom -O ihex $< $@

%_eeprom.srec: %.elf
	$(OBJCOPY) -j .eeprom -O srec $< $@

%_eeprom.bin: %.elf
	$(OBJCOPY) -j .eeprom -O binary $< $@


# Programming the device with avrdude

# Programming speed needs to be adjusted to the target's current clock
# Rule of thumb: must be less than 1/4 of current MCU clock speed.
PRGSPEED       = -B 30kHz

# This is mainly concerned with setting up the correct clock configuration.
FUSEOPTS       = -Ulfuse:w:0xda:m

dude: $(PRG).hex
	avrdude -c usbtiny -p $(MCU_TARGET) $(PRGSPEED) $(FUSEOPTS) -U flash:w:$<
