#include "twi.h"

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

Twi::Twi() {}

void Twi::init() {
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
/* Start condition:
 *
 * SDA and SCL are set HIGH
 * SDA is set LOW
 * SCL is set LOW
 *
 * In the original you can pass in an int for "read"
 * 
 */
bool Twi::start (uint8_t address, bool read) {
  uint8_t addressRW = address << 1;

  if (read) {
    addressRW |= 0x01;
  }

  /* Establish I2C start conditions */

  // Set SCL HIGH
  PORTB |= 1 << PORT_USI_SCL;

  // Verify that SCL becomes high.
  while (!(PINB & 1 << PIN_USI_SCL)) {
    _delay_us(DELAY_T4TWI);
  }

  // Set SDA LOW
  PORTB &= ~(1 << PORT_USI_SDA);
  _delay_us(DELAY_T4TWI);

  // Set SCL LOW
  PORTB &= ~(1 << PORT_USI_SCL);

  // Set SDA to HIGH
  PORTB |= 1 << PORT_USI_SDA;

  /* When USISIF on USISR is HIGH this indicates that a start condition has been observed
   * If we haven't observed the start condition then something is wrong and we'll bail
   */
  if (!(USISR & 1 << USISIF)) {
    return false;
  }

  /* Write address */

  // Set SCL LOW
  PORTB &= ~(1 << PORT_USI_SCL);

  // Put address on the USI Data Register
  USIDR = addressRW;

  // Send 8 bits on bus.
  this->transfer(USISR_8bit);

  /* Clock and verify (N)ACK from slave */

  DDRB &= ~(1 << DD_USI_SDA); // Enable SDA as input.

  if (this->transfer(USISR_1bit) & 1 << TWI_NACK_BIT) {
    // No ACK
    return false;
  }

  // Start successfully completed
  return true;
}

/* In cases where you want to read multiple bytes off of the line
 * need to generate an ACK over and over until completion
 * then NACK.
 */
uint8_t Twi::read_one() {
  return this->read(END);
}

/* ACK = false for end of reading
 * ACK = true for more to read
 */
uint8_t Twi::read(bool more) {
  // Read a byte
  DDRB &= ~(1 << DD_USI_SDA); // Enable SDA as input.
  uint8_t data = this->transfer(USISR_8bit);

  if (more) {
    // Ack: More to read
    USIDR = ACK;
  } else {
    // NACK: completion of read
    USIDR = NACK;
  }

  this->transfer(USISR_1bit);
  return data;
}

void Twi::readn(uint8_t array[], uint8_t n) {
  while (n > 0) {
    array[n] = this->read(MORE);
    n--;
  }

  array[0] = this->read(END);
}

bool Twi::write(uint8_t data) {
  // Write a byte 
  PORTB &= ~(1 << PORT_USI_SCL); // Pull SCL LOW.
  USIDR = data; // Setup data.
  this->transfer(USISR_8bit); // Send 8 bits on bus.

  /* Clock and verify (N)ACK from slave */
  DDRB &= ~(1 << DD_USI_SDA); // Enable SDA as input.

  if (this->transfer(USISR_1bit) & 1 << TWI_NACK_BIT) {
    return false;
  }

  return true;                                // Write successfully completed
}

/* Moves bytes on the bus
 *
 * USISR: Status register
 * USICR: Control Register
 *
 * See USISR_8bit and USISR_1bit as preconfigured
 * statuses for transfering a byte and a bit respectively
 */
uint8_t Twi::transfer(uint8_t status) {
  // Set USISR according to status
  USISR = status;

  // Prepare clocking
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

    /* USIOIF is the counter overflow interupt flag
     * when we set the status register earlier we gave it a number of 
     * edges to count (16 for 8 bits, 2 for 1 bit) this bit will
     * be set when that counter overflows thus observing transfer completion
     */
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

void Twi::stop (void) {
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
