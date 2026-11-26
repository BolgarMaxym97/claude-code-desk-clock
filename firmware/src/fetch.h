// The clock fetches its own outside data over plain HTTP (no TLS on the ESP8266), so it works while the Mac sleeps:
// weather from OpenWeather every 15 min, the air-raid alert for the chosen region from ubilling every 30 s.
#pragma once
#include <Arduino.h>
#include "info.h"

Info &infoState();
void fetchTick(uint32_t now);   // one request at a time, only with Wi-Fi up and the source configured
void fetchSoon();               // settings changed: fetch again right away
