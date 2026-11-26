#pragma once
#include <TFT_eSPI.h>

// ST7789 init tuned for this panel (P2 voltages/gamma + MADCTL 0x00 + GAMSET 1.0).
void panelInit(TFT_eSPI &tft);
// Backlight in percent (GPIO5, active-LOW PWM).
void setBrightness(uint8_t pct);
