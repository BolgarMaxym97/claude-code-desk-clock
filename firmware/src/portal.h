// Captive setup page for the "Claude-Clock" access point: list networks, enter password, check, save.
#pragma once
#include <ESP8266WebServer.h>

void portalBegin(ESP8266WebServer &server);
bool portalRedirect();  // true if the request came from the setup AP and was redirected to /setup
