# simavr-based simulator for DCF77 clock firmware

MCU = atmega328p

AVR_DEBUG_FLAG = -gdwarf-2
AVR_CPPFLAGS = -std=c11 -fsigned-char -Wall -g -Og --short-enums

CPPFLAGS = -g -Og
ADD_LIBS = -lutil
LDFLAGS = -pthread

BINS = simulator
AVR_BINS = firmware.axf

all: $(BINS) $(AVR_BINS)

OBJS_simulator = simulator.o \
                 button.o dcf77_rcvr.o dht22_sensor.o lcd_display.o

OBJS_firmware = $(patsubst ../src/%.c, %.avro, $(wildcard ../src/*.c)) \
                simavr.avro

OBJS = $(OBJS_simulator) $(OBJS_firmware) *.vcd

simulator: $(OBJS_simulator)
firmware.axf: $(OBJS_firmware)

VPATH = ../src

include makefile.common
