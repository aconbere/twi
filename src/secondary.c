#include "secondary.h"

/*
 * on ATtiny85 USI runs on PortB (the only port)
 * SDA and SCL lines are the two wires they map to
 * pins PORTB0 and PORTB2 respectively
 *
 * #define SDA PORTB0
 * #define SCL PORTB2
 *
 * USI_START_vect
 *
 * USISR is the USI status register
 * USICR is the USI control register
 * DDRB: Data Direction Register
 * USIDR: USI Data Register
 */

#define PORT_USI_SDA PORTB0
#define PORT_USI_SCL PORTB2

#define PIN_USI_SDA PINB0
#define PIN_USI_SCL PINB2

#define DD_USI_SDA DDB0
#define DD_USI_SCL DDB2

#define ADDRESS = 0x04

void secondary_init() {
  // Enable pullup on SDA.
  PORTB |= 1 << PORT_USI_SDA;

  // Enable pullup on SCL.
  PORTB |= 1 << PORT_USI_SCL;

  // Enable SDA as input.
  DDRB |= 0 << DD_USI_SDA;

  // Enable SCL as input.
  DDRB |= 0 << DD_USI_SCL;

  // Preload data register with "released level" data.
  USIDR = 0xFF;

  USICR = 0 << USISIE | 0 << USIOIE | // Disable Interrupts.
          1 << USIWM1 | 0 << USIWM0 | // Set USI in Two-wire mode.
          1 << USICS1 | 0 << USICS0 | 1 << USICLK | // Software stobe as counter clock source
          0 << USITC;

  USISR = 1 << USISIF | 1 << USIOIF | 1 << USIPF | 1 << USIDC | // Clear flags,
          0x0 << USICNT0; // and reset counter.
}

bool secondary_start(uint8_t my_address) {
  /* Loop until the USISIF flag is set */
  while (!(USISR & 1 << USISIF)) {
  }

  /* When USISIF is set we've observed a start condiation
   * When that happens we expect to receive an address.
   *
   * Ensure that SDA is set as input, then read a byte.
   *
   * We check that the address is equal to the one set in
   * ADDRESS. And if so we respond with an ACK.
   */
  DDRB &= ~(1 << DD_USI_SDA);

  uint8_t address = secondary_transfer(USISR_8bit);

  if (address == my_address) {
    // Prepare to generate ACK
    USIDR = 0xFF;

    // Generate ACK/NACK.
    secondary_transfer(USISR_1bit);
    return true;
  }

  return false;
}


uint8_t secondary_read() {
  /* Read a byte */

  // Enable SDA as input.
  DDRB &= ~(1 << DD_USI_SDA);
  uint8_t data = secondary_transfer(USISR_8bit);

  // Prepare to generate ACK
  USIDR = 0xFF;

  // Generate ACK/NACK.
  secondary_transfer(USISR_1bit);

  return data;
}

bool secondary_write(uint8_t data) {
  // Write a byte 

  // Pull SCL LOW.
  PORTB &= ~(1 << PORT_USI_SCL);
  // Setup data.
  USIDR = data;

  // Send 8 bits on bus.
  secondary_transfer(USISR_8bit);

  /* Clock and verify (N)ACK from Secondary */

  // Enable SDA as input.
  DDRB &= ~(1 << DD_USI_SDA);

  if (secondary_transfer(USISR_1bit) & 1 << secondary_NACK_BIT) {
    return false;
  }

  // Write successfully completed
  return true;
}

uint8_t secondary_transfer(uint8_t status) {
  USISR = status; // Set USISR according to data.

  // Prepare clocking.
  status = 0 << USISIE | 0 << USIOIE | // Interrupts disabled
           1 << USIWM1 | 0 << USIWM0 | // Set USI in Two-wire mode.
           1 << USICS1 | 0 << USICS0 | 1 << USICLK | // Software clock strobe as source.
           1 << USITC; // Toggle Clock Port.

  do {
    _delay_us(DELAY_T2TWI);

    // Generate positive SCL edge.
    USICR = status;

    // Wait for SCL to go high.
    while (!(PINB & 1 << PIN_USI_SCL)) {
      _delay_us(DELAY_T4TWI);
    }

    // Generate negative SCL edge.
    USICR = status;
  } while (!(USISR & 1 << USIOIF)); // Check for transfer complete.

  _delay_us(DELAY_T2TWI);

  // Read out data.
  status = USIDR;

  // Release SDA.
  USIDR = 0xFF;

  // Enable SDA as output.
  DDRB |= (1 << DD_USI_SDA);

  // Return the data from the USIDR
  return status;
}
