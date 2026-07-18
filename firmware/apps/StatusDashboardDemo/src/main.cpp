#include <Arduino.h>
#include "StatusDashboard.h"

StatusDashboard dashboard(41, 42);  // SDA=41, SCL=42

void setup() {
    Serial.begin(921600);
    delay(500);

    dashboard.begin();

    Serial.println("StatusDashboard service demo — 4 tasks, 3 scrolling lines");
    Serial.println("  Line 0: u8g2_font_5x8_tf, lines 1-3: default u8g2_font_5x7_tf");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
