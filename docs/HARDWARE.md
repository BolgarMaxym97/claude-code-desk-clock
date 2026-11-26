# Hardware

Tested with the AliExpress "Smart weather clock" cube (~5 cm, GeekMagic SmallTV look-alike) whose stock firmware is
"SD PRO" (v1.1.x). Clocks from other sellers that look the same often use the same board. If yours has the same
chips and pins, it will very likely work. Other variants are untested; try them at your own risk.

| Part | Detail |
|---|---|
| MCU | ESP-12F module (ESP8266, 4 MB flash) |
| Screen | 1.54" IPS 240×240, ST7789, no CS line |
| Screen pins | MOSI 13, SCLK 14, DC 0, RST 2, SPI mode 3 |
| Backlight | GPIO 5, active-low PWM |
| USB | Power only (the USB-serial chip is not fitted), so the firmware is installed over Wi-Fi |

**How to check your clock:** it should have a web page with a firmware update (OTA) section that accepts `.bin`
files. If the screen stays black after the update but the clock still shows up on your network, the screen wiring
is different. Write down the board markings and open an issue.

## Flash layout (what the firmware touches)

The stock firmware uses the "4M2M" layout. This firmware:

- installs into the normal application slot (below 0x200000) through the regular update mechanism;
- never formats or erases the stock data area (0x200000–0x3FA000), where the stock firmware keeps its pictures;
- uses only 4 sectors at its very end, 0x3F6000–0x3F9FFF. They store the Wi-Fi network and the settings (token,
  timezone, birthday) as A/B copies with CRC. Before writing, it checks that these sectors are empty or already
  its own. If anything else is found there, nothing is written and the clock runs without saving.
- never touches the bootloader, EEPROM (0x3FA000) or the Wi-Fi SDK sectors.

There is also a crash-loop guard: after 3 crashes in a row the clock starts in a safe mode with flash writes
off. Its update page (`/update`) stays available, so you can always install another firmware over the network.
