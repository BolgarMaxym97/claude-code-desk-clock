#include "panel.h"
#include <initializer_list>

static const uint8_t BL_PIN = 5;

static void cmd(TFT_eSPI &tft, uint8_t c, std::initializer_list<uint8_t> data) {
    tft.writecommand(c);
    for (uint8_t d : data) tft.writedata(d);
}

void panelInit(TFT_eSPI &tft) {
    pinMode(BL_PIN, OUTPUT);
    analogWriteRange(255);
    setBrightness(80);
    tft.init();
    cmd(tft, 0x01, {});  // SWRESET -> panel defaults, then our own values
    delay(150);
    cmd(tft, 0x11, {});  // SLPOUT
    delay(120);
    cmd(tft, 0x3A, {0x55});  // 16-bit colour
    cmd(tft, 0xB7, {0x35});
    cmd(tft, 0xBB, {0x19});
    cmd(tft, 0xC0, {0x2C});
    cmd(tft, 0xC2, {0x01});
    cmd(tft, 0xC3, {0x12});
    cmd(tft, 0xC4, {0x20});
    cmd(tft, 0xC6, {0x0F});
    cmd(tft, 0xD0, {0xA4, 0xA1});
    cmd(tft, 0xE0, {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23});
    cmd(tft, 0xE1, {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23});
    cmd(tft, 0x21, {});  // INVON (panel is natively inverted)
    cmd(tft, 0x13, {});  // NORON
    cmd(tft, 0x29, {});  // DISPON
    delay(20);
    tft.setRotation(0);
    cmd(tft, 0x36, {0x00});  // MADCTL: setRotation leaves R/B swapped after SWRESET
    cmd(tft, 0x26, {0x08});  // GAMSET curve 1.0 (acts inverted under INVON): darker darks
}

void setBrightness(uint8_t pct) {
    if (pct > 100) pct = 100;
    analogWrite(BL_PIN, 255 - pct * 255 / 100);
}
