#ifndef ATTINYARCADE_H
#define ATTINYARCADE_H

#include "Arduino.h"
#include "i2c.h"

//display size
#define WIDTH  128
#define HEIGHT 64

//buttons and speaker
#define LEFT_BUTTON_BIT  PB0
#define RIGHT_BUTTON_BIT PB2
#define LEFT_BUTTON  (1 << LEFT_BUTTON_BIT)
#define RIGHT_BUTTON (1 << RIGHT_BUTTON_BIT)
#define SPEAKER_BIT      PB1

#define FPS_TO_MS(FPS) (1000 / FPS)

//sprite modes
#define SM_OVER 0
#define SM_MASK 1

void core_init();
void core_idle();
uint16_t core_millis();
void core_wait(uint16_t ms);
void core_restart();

void powerDown(uint8_t WakeUpButtons = 0);
uint8_t buttons();

void display_sync(uint8_t frameDuration);
void display_commandMode();
void display_writeCommand(uint8_t cmd);
void display_dataMode();
void display_writeData(uint8_t data);
void display_init(byte mode);
void display_off();
void display(void (*drawBackground)(uint8_t));

void sprites_clear();

#endif
