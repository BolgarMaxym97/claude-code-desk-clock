// Settings page on the clock: GET / (outside the Wi-Fi setup), GET /api/settings, POST /api/geocode?q=city.
#pragma once
#include <ESP8266WebServer.h>

void webuiBegin(ESP8266WebServer &server);
void webuiPage();  // the page itself (the "/" route lives in api.cpp next to the setup redirect)
