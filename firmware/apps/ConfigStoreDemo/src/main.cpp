#include <Arduino.h>
#include "ConfigStore.h"

ConfigStore config(41, 42);  // SDA=41, SCL=42

void setup() {
    Serial.begin(921600);
    delay(500);

    config.begin("config");
}

void loop() {
    config.handleSerial();
    delay(100);
}
