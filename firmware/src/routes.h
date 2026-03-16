#pragma once

#include <ESPAsyncWebServer.h>
#include "escpos.h"

void setupRoutes(AsyncWebServer &server, EscPosWriter *printer);
