/*
 * StatusPanel — Integration Test
 *
 * Composes StatusDashboard (scrolling OLED telemetry) with LedRGBController
 * (NeoPixel status indicator) into a system health monitoring panel.
 *
 * The OLED shows live uptime, heap, PSRAM, WiFi RSSI, and SDK version.
 * The NeoPixel glows green when heap is healthy (>100KB free), red when
 * memory is low (<50KB free), and yellow in between.
 *
 * Run:
 *   uv run pio run --target upload
 *   uv run pio run --target upload --target monitor
 */

#include <Arduino.h>
#include <StatusDashboard.h>
#include <LedRGBController.h>

StatusDashboard  dashboard(41, 42);
LedRGBController  led(48);  // on-board NeoPixel

void setup() {
    Serial.begin(921600);
    delay(500);

    dashboard.setFirmwareVersion("StatusPanel v1.0");
    dashboard.begin();

    led.begin();
    led.setBrightness(25);
    led.indicateBoot();

    Serial.println("StatusPanel — System Health Monitor");
    Serial.println("  OLED: scrolling dashboard, NeoPixel: heap status");
}

void loop() {
    // Map free heap to LED color
    size_t freeHeap = ESP.getFreeHeap();

    if (freeHeap > 100000) {
        led.turn_Green();       // Healthy
    } else if (freeHeap > 50000) {
        led.turn_Color(255, 200, 0);  // Warning (yellow)
    } else {
        led.turn_Red();         // Critical
    }

    // Print heap status periodically
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 15000) {
        lastPrint = millis();
        Serial.printf("[Heap: %u/%u free] [WiFi: %d dBm]\n",
            freeHeap, ESP.getHeapSize(), WiFi.RSSI());
    }

    delay(1000);
}
