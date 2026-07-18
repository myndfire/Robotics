#include <Arduino.h>
#include "MoistureMonitor.h"

MoistureMonitor monitor(1, 41, 42);  // sensorPin=1, SDA=41, SCL=42
StorageController storage;

void setup() {
    Serial.begin(921600);
    delay(500);

    monitor.begin(storage);

    Serial.println("MoistureMonitor service demo");
    Serial.println("  Line 0: u8g2_font_5x8_tf, lines 1-3: default u8g2_font_5x7_tf");
    Serial.printf("Sensor AO: GPIO1 | OLED: SDA=41 SCL=42\n");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
