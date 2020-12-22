#ifndef Twi_h
#define Twi_h

#include "stdbool.h"
#include <stdint.h>
#include <avr/io.h>
#include <util/delay.h>

// >1.3us
#define DELAY_T2TWI 2

// >0.6us
#define DELAY_T4TWI 1

// Bit position for (N)ACK bit.
#define TWI_NACK_BIT 0

// Prepare register value to: Clear flags, and set USI to shift 8 bits i.e. count 16 clock edges.
const unsigned char USISR_8bit = 1<<USISIF | 1<<USIOIF | 1<<USIPF | 1<<USIDC | 0x0<<USICNT0;

// Prepare register value to: Clear flags, and set USI to shift 1 bit i.e. count 2 clock edges.
const unsigned char USISR_1bit = 1<<USISIF | 1<<USIOIF | 1<<USIPF | 1<<USIDC | 0xE<<USICNT0;

void twi_init(void);
uint8_t twi_read(void);
bool twi_write(uint8_t data);
bool twi_start(uint8_t address, bool read);
void twi_stop(void);
uint8_t twi_transfer(uint8_t status);

#endif
