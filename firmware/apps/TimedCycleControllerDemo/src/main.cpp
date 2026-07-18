#include <Arduino.h>
#include "TimedCycleController.h"

TimedCycleController controller(4, 41, 42);  // relayPin=4, SDA=41, SCL=42
StorageController storage;

void setup() {
    Serial.begin(921600);
    delay(500);

    storage.begin("timedcycle");
    controller.begin(storage);

    Serial.println("TimedCycleController service demo");
    Serial.println("  Line 0: u8g2_font_5x8_tf, lines 1-3: default u8g2_font_5x7_tf");
    Serial.printf("Relay: GPIO4 | OLED: SDA=41 SCL=42\n");
    Serial.printf("Cycle: ON=%ds OFF=%ds\n", controller.getOnDuration(), controller.getOffDuration());
    Serial.println("Commands: on=N  off=N  s (status)");
}

void loop() {
    controller.handleSerial();
    delay(100);
}
