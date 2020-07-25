/*******************************************************************************
* I2C bitbang library with optimized Assembly
* Mr. Blinky Dec 2018
* MIT Licence
*******************************************************************************/

#ifndef I2C_H
#define I2C_H

#include "Arduino.h"

//port values
#define I2C_SCL   PB4
#define I2C_SDA   PB3

#define I2C_PORT  PORTB
#define I2C_DDR   DDRB
#define I2C_PIN   PINB

//slave addresses
#define I2C_DISPLAY_SA  0x78 /* default */
#define I2C_DISPLAY_SA0 0x78      
#define I2C_DISPLAY_SA1 0x7A

#define I2C_MEMORY_SA   0xA0 /* default */
#define I2C_MEMORY_SA0  0xA0
#define I2C_MEMORY_SA1  0xA2
#define I2C_MEMORY_SA2  0xA4
#define I2C_MEMORY_SA3  0xA6
#define I2C_MEMORY_SA4  0xA8
#define I2C_MEMORY_SA5  0xAA
#define I2C_MEMORY_SA6  0xAC
#define I2C_MEMORY_SA7  0xAE

//port states
#define I2C_SDA_HIGH() I2C_PORT |=  (1 << I2C_SDA)
#define I2C_SCL_HIGH() I2C_PORT |=  (1 << I2C_SCL)
#define I2C_SDA_LOW()  I2C_PORT &= ~(1 << I2C_SDA)
#define I2C_SCL_LOW()  I2C_PORT &= ~(1 << I2C_SCL)

//port directions
#define I2C_SDA_AS_INPUT()  I2C_DDR &= ~(1 << I2C_SDA)
#define I2C_SCL_AS_INPUT()  I2C_DDR &= ~(1 << I2C_SCL)
#define I2C_SDA_AS_OUTPUT() I2C_DDR |= (1 << I2C_SDA)
#define I2C_SCL_AS_OUTPUT() I2C_DDR |= (1 << I2C_SCL)

void i2c_init();
void i2c_start(uint8_t slaveAddress);
void i2c_stop();
void i2c_sendByte(uint8_t byte);

#endif