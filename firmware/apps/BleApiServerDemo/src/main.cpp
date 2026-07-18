#include <Arduino.h>

#ifdef DORHEA_BOARD
#include "BleApiServer.h"
BleApiServer server(21, NEO_RGB + NEO_KHZ800);
#else
#include "BleApiServer.h"
BleApiServer server;
#endif

void setup() {
    Serial.begin(921600);
    delay(500);
    server.begin();
}

void loop() {
    delay(100);
}
