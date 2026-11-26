// HTTP API (spec 6.5). /api/* need header X-Clock-Token; /update uses Basic auth clock:<token>.
#pragma once
#include <ESP8266WebServer.h>
#include "model.h"

void apiBegin(ESP8266WebServer &server, Model &model);
