/*******************************************************************************
* I2C bitbang library with optimized Assembly
* Mr. Blinky Dec 2018
* MIT Licence
*******************************************************************************/

#include "i2c.h"

void i2c_init()
{
  I2C_SDA_HIGH();    
  I2C_SCL_HIGH(); 
  I2C_SDA_AS_OUTPUT();  
  I2C_SCL_AS_OUTPUT();  
}

void i2c_start(uint8_t slaveAddress)
{
  I2C_SCL_HIGH();
  I2C_SDA_HIGH();
  I2C_SDA_LOW();
  I2C_SCL_LOW();
  i2c_sendByte(slaveAddress);
}

void i2c_stop()
{
  I2C_SCL_LOW();
  I2C_SDA_LOW();
  I2C_SCL_HIGH();
  I2C_SDA_HIGH();
}

void i2c_sendByte(uint8_t byte)
{
  asm volatile(
    "    sec                  \n" // Use carry flag to count 8 bits
    "    rol  %[byte]         \n" // counter bit in, MSB bit out
    "1:                       \n"
    "    sbi  %[port], %[sda] \n" // pre-emptively set data bit
    "    brcs 2f              \n" // skip if '1' bit data
    "    cbi  %[port], %[sda] \n" // else clear data bit
    "2:                       \n"
    "    sbi  %[port], %[scl] \n" // clock high
    "    lsl  %[byte]         \n" // shift out next bit, move counter bit
    "    cbi  %[port], %[scl] \n" // clock low
    "    brne 1b              \n" // loop until counter bit shifted out
    "                         \n"
    "    sbi  %[port], %[sda] \n" // data high for ack bit
    "    sbi  %[port], %[scl] \n" // ack clock pulse
    "    cbi  %[port], %[scl] \n" //
    : [byte] "+&r" (byte)
    : [port] "I" (_SFR_IO_ADDR(I2C_PORT)),
      [sda]  "I" (I2C_SDA),
      [scl]  "I" (I2C_SCL)
    );
}
