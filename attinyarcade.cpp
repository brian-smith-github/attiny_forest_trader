#include <avr/sleep.h>
#include "attinyarcade.h"

extern uint32_t millis_timer_millis;

uint8_t  wakeUpButtons;

void core_init()
{
  //setup hardware pins pins 
  PORTB = (HIGH << I2C_SCL) | 
          (HIGH << I2C_SDA) |
          (LOW  << RIGHT_BUTTON_BIT) | 
          (LOW  << LEFT_BUTTON_BIT) |
          (LOW  << SPEAKER_BIT);
          
  DDRB  = (OUTPUT << I2C_SCL) |
          (OUTPUT << I2C_SDA) |
          (INPUT  << RIGHT_BUTTON_BIT) |
          (INPUT  << LEFT_BUTTON_BIT) |
          (OUTPUT << SPEAKER_BIT);
}


ISR(PCINT0_vect,ISR_NAKED) //Pin change interrupt that saves the wake up button state
                           //NAKED is used so compiler doesn't add any wrapper code
{
  asm volatile(
    "  push r16           \n" //save register (using higher register for ANDI instruction)
    "  in   r16, __SREG__ \n" //save status register because of ANDI instruction changing flags
    "  push r16           \n"
    "  in   r16, %[port]  \n" //read buttons port
    "  andi r16, %[mask]  \n" //drop unwanted bits
    "  sts  %[wub], r16   \n" //sabe buttons state
    "  pop  r16           \n" //restore status register
    "  out  __SREG__, r16 \n"
    "  pop  r16           \n" //restore register
    "  reti               \n" //return from interrupt
    :
    : [port] "I" _SFR_IO_ADDR(PINB),
      [mask] "I" (LEFT_BUTTON | RIGHT_BUTTON),
      [wub]  ""  (&wakeUpButtons)
  );
}

void powerDown(uint8_t WakeUpButtons)
{
  _delay_ms(200);
  if (WakeUpButtons == 0) WakeUpButtons = LEFT_BUTTON | RIGHT_BUTTON;
  PCMSK = (WakeUpButtons); //set wake up buttons change mask
  GIMSK = 1 << PCIE; // enable pin change interrupt
  display_writeCommand(0xAE); //put display in low power mode
  ADCSRA &= ~(1 << ADEN); //make sure ADC is off
  MCUCR = SLEEP_MODE_PWR_DOWN | (1 << SE);//enable power down mode
  sei();       //enable interrupts
  sleep_cpu(); //power down now
  MCUCR = 0;   //disable sleep mode
  GIMSK = 0;   //disable pin change interrupt
  display_init(0);
  while (buttons() != 0);
  _delay_ms(500);    
}

uint8_t buttons()
{ 
  return PINB & (RIGHT_BUTTON | LEFT_BUTTON);
}


void display_commandMode()
{
  i2c_start(I2C_DISPLAY_SA);
  i2c_sendByte(0x00); // D/#C = '0'
}

void display_writeCommand(uint8_t cmd)
{
  display_commandMode();
  i2c_sendByte(cmd);
  i2c_stop();
}

void display_dataMode()
{
  i2c_start(I2C_DISPLAY_SA);
  i2c_sendByte(0x40); // D/#C = '1'
}

void display_writeData(uint8_t data)
{
  display_dataMode();
  i2c_sendByte(data);
  i2c_stop();
}

const uint8_t PROGMEM display_init_data[] = {
0x40,             // set display startline to 0 (default)    
0x81, 0xCF,       // set contrast to 0xCF
0xA1,             // enable segment remap
0xC8,             // enable com output direction remap
0x20, 0x00,       // set memory addressing mode to horizontal addressing mode
0x21, 0x00, 0x7F, // set column start and end
0x22, 0x00, 0x07, // set page start and end
0xD9, 0xF1,       // set precharge ratio to phase 1 = 1 DCLKs, Phase 2 = 15 DCLKs
0x8D, 0x14,       // set charge pump setting to enabled
0xAF              // display on
};

void display_init(byte mode)
{
  byte a;
  for (int i = 0; i < sizeof(display_init_data); i++)
  {  
    a=pgm_read_byte(&display_init_data[i]);
    if (mode && i==6) a=1; // vertical mode intead of horizontal
    display_writeCommand(a);
  }
  display_dataMode();
}
