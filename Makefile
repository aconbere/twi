DEVICE			= attiny85
CLOCK				= 8000000
PROGRAMMER	= usbtiny
PORT				= usb
BAUD				= 19200
CC					= avr-gcc -Wall -Os -DF_CPU=$(CLOCK) -mmcu=$(DEVICE) -Ilib
CPP					= avr-g++ -std=c++11 -Wall -Os -DF_CPU=$(CLOCK) -mmcu=$(DEVICE) -Ilib -fno-exceptions -ffunction-sections -fdata-sections -Wl,--gc-sections

.DEFAULT_GOAL := all

build/primary.o: src/primary.c src/primary.h
	$(CC) -o ./build/primary.o -c src/primary.c

.PHONY: all
all: build/primary.o

.PHONY: clean
clean:
	$(RM) ./build/*
