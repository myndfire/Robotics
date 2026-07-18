#include <Arduino.h>
#include "CameraWebServer.h"

CameraWebServer server;

void setup() {
    Serial.begin(921600);
    delay(500);
    server.begin();
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
