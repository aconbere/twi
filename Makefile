DEVICE			= attiny85
CLOCK				= 8000000
PROGRAMMER	= usbtiny
PORT				= usb
BAUD				= 19200
CC					= avr-gcc -Wall -Os -DF_CPU=$(CLOCK) -mmcu=$(DEVICE) -Ilib
CPP					= avr-g++ -std=c++11 -Wall -Os -DF_CPU=$(CLOCK) -mmcu=$(DEVICE) -Ilib -fno-exceptions -ffunction-sections -fdata-sections -Wl,--gc-sections

.DEFAULT_GOAL := all

build/twi.o: src/twi.cpp src/twi.h
	$(CPP) -o ./build/main.o -c src/twi.cpp

.PHONY: all
all: build/twi.o

.PHONY: clean
clean:
	$(RM) ./build/*
