# Third-party notices

The firmware embeds or links the following. Each keeps its own license.

| Component | Use | License |
|---|---|---|
| [Inter](https://github.com/rsms/inter) (Rasmus Andersson) | UI font, converted to bitmap glyphs in `firmware/include/fonts.h` | SIL Open Font License 1.1 |
| [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) | Time / monospace font, converted likewise | SIL Open Font License 1.1 |
| [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) (Bodmer) | Display driver | FreeBSD / MIT (see its license.txt) |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) (Benoit Blanchon) | JSON | MIT |
| [QRCode](https://github.com/ricmoo/QRCode) (Richard Moore) | Wi-Fi setup QR code | MIT |
| [ESP8266 Arduino core](https://github.com/esp8266/Arduino) | Framework, Wi-Fi, web server, OTA | LGPL 2.1 (libraries), see the project |

The `claude-clock` host program uses only the Go standard library (BSD-3-Clause).

The fonts in `fonts.h` are bitmap renderings of subsets of the original fonts (Latin, Ukrainian Cyrillic and a few
symbols). They are distributed under the SIL OFL 1.1 like the originals. The reserved font names are not used for
the derived data.
