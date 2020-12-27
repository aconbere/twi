#include "primary.h"

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

void primary_init() {
  // Enable pullup on SDA.
  PORTB |= 1 << PORT_USI_SDA;

  // Enable pullup on SCL.
  PORTB |= 1 << PORT_USI_SCL;

  // Enable SDA as output.
  DDRB |= 1 << DD_USI_SDA;

  // Enable SCL as output.
  DDRB |= 1 << DD_USI_SCL;

  // Preload data register with "released level" data.
  USIDR = 0xFF;

  USICR = 0 << USISIE | 0 << USIOIE | // Disable Interrupts.
          1 << USIWM1 | 0 << USIWM0 | // Set USI in Two-wire mode.
          1 << USICS1 | 0 << USICS0 | 1 << USICLK | // Software stobe as counter clock source
          0 << USITC;

  USISR = 1 << USISIF | 1 << USIOIF | 1 << USIPF | 1 << USIDC | // Clear flags,
          0x0 << USICNT0; // and reset counter.

}

// Start transmission by sending address
bool primary_start (uint8_t address) {
  uint8_t addressRW = address << 1;

  // set rw bit to WRITE 
  addressRW |= 0x00;

  /* Release SCL to ensure that (repeated) Start can be performed */
  PORTB |= 1 << PORT_USI_SCL;

  // Verify that SCL becomes high.
  while (!(PINB & 1 << PIN_USI_SCL)) {
    _delay_us(DELAY_T4TWI);
  }

  /* Generate Start Condition
   *
   * To initiate the address frame, the controller device leaves SCL high and
   * pulls SDA low. This puts all peripheral devices on notice that a
   * transmission is about to start. If two controllers wish to take ownership
   * of the bus at one time, whichever device pulls SDA low first wins the race
   * and gains control of the bus. It is possible to issue repeated starts,
   * initiating a new communication sequence without relinquishing control of
   * the bus to other controller(s);
   */

  // Force SDA LOW.
  PORTB &= ~(1 << PORT_USI_SDA);
  _delay_us(DELAY_T4TWI);

  // Pull SCL LOW.
  PORTB &= ~(1 << PORT_USI_SCL);

  // Release SDA.
  PORTB |= 1 << PORT_USI_SDA;

  /* USISIF is a flag that is flipped by the USI function on the AVR chip
   * if we have successfully triggered a start condition the controller
   * will detect the start condition and set this flag
   *
   * see: 15.3.4 Two Wire Mode in the manual
   */
  if (!(USISR & 1 << USISIF)) {
    return false;
  }

  /* Write address */

  // Pull SCL LOW.
  PORTB &= ~(1 << PORT_USI_SCL);

  // Setup data.
  USIDR = addressRW;

  // Send 8 bits on bus.
  primary_transfer(USISR_8bit);

  /* Clock and verify (N)ACK from primary */

  // Enable SDA as input.
  DDRB &= ~(1 << DD_USI_SDA);

  if (primary_transfer(USISR_1bit) & 1 << PRIMARY_NACK_BIT) {
    // No ACK
    return false;
  }

  // Start successfully completed
  return true;
}

uint8_t primary_read() {
  // Read a byte

  // Enable SDA as input.
  DDRB &= ~(1 << DD_USI_SDA);
  uint8_t data = primary_transfer(USISR_8bit);

  // Prepare to generate ACK (or NACK in case of End Of Transmission)
  USIDR = 0xFF;

 // Generate ACK/NACK.
  primary_transfer(USISR_1bit);

  return data;
}

bool primary_write(uint8_t data) {
  /* Write a byte */

  // Pull SCL LOW.
  PORTB &= ~(1 << PORT_USI_SCL);

  // Setup data.
  USIDR = data;

  // Send 8 bits on bus.
  primary_transfer(USISR_8bit);

  /* Clock and verify (N)ACK from secondary */

  // Enable SDA as input.
  DDRB &= ~(1 << DD_USI_SDA);

  if (primary_transfer(USISR_1bit) & 1 << PRIMARY_NACK_BIT) {
    return false;
  }

  // Write successfully completed
  return true;
}

uint8_t primary_transfer(uint8_t status) {
  // Set USISR according to data.
  USISR = status;

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

    // Check for transfer complete.
  } while (!(USISR & 1 << USIOIF)); 

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

void primary_stop (void) {
  // Pull SDA low.
  PORTB &= ~(1 << PORT_USI_SDA);

  // Release SCL.
  PORTB |= 1 << PORT_USI_SCL;

  // Wait for SCL to go high.
  while (!(PINB & 1 << PIN_USI_SCL)) {
    _delay_us(DELAY_T4TWI);
  }

  // Release SDA.
  PORTB |= 1 << PORT_USI_SDA;
  _delay_us(DELAY_T2TWI);
}
