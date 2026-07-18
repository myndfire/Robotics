/*
 * MoisturePump — Integration Test
 *
 * Composes MoistureMonitor and TimedCycleController into a complete
 * automated irrigation system.  The MoistureMonitor reads soil moisture
 * every 5 seconds.  When the soil is DRY, the TimedCycleController
 * activates the pump on a configurable duty cycle (default 3s ON / 7s OFF).
 * When the soil is WET, the pump stops.
 *
 * Both services run their own FreeRTOS tasks — this main.cpp only ties
 * them together via the shared StorageController and control loop.
 *
 * Hardware needed:
 *   - ESP32-S3 DevKitC-1
 *   - LM393 soil moisture sensor (AO → GPIO 1)
 *   - Relay module (IN → GPIO 4)
 *   - SSD1306 128x32 OLED (SDA=41, SCL=42)
 *
 * Run:
 *   uv run pio run --target upload
 *   uv run pio run --target upload --target monitor
 */

#include <Arduino.h>
#include <MoistureMonitor.h>
#include <TimedCycleController.h>

MoistureMonitor      moisture(1, 41, 42);
TimedCycleController  pump(4, 41, 42);
StorageController             storage;

void setup() {
    Serial.begin(921600);
    delay(500);

    storage.begin("irrigation");
    moisture.begin(storage);
    pump.begin(storage);

    Serial.println("MoisturePump — Automated Irrigation");
    Serial.println("  Moisture monitor: GPIO 1, relay pump: GPIO 4");
    Serial.println("  Reading every 5s, pump activates when DRY");
}

void loop() {
    // Check moisture status
    if (moisture.isDry() && !pump.isRelayOn()) {
        Serial.println("Soil DRY — starting pump cycle");
    }
    if (moisture.isWet() && pump.isRelayOn()) {
        Serial.println("Soil WET — pump cycle continues until OFF phase");
    }

    // Handle serial commands for pump duration tuning
    pump.handleSerial();

    // Print status periodically
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 10000) {
        lastPrint = millis();
        Serial.printf("[Moisture: %d%% %s] [Pump: %s, %ds remaining]\n",
            moisture.getMoisturePercent(),
            moisture.getStatus(),
            pump.isRelayOn() ? "ON" : "OFF",
            pump.getPhaseRemaining());
    }

    delay(100);
}
