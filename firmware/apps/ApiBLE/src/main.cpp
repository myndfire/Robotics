/*
 * ApiBLE — Unified BLE firmware for ESP32-S3-CAM
 *
 * Combines Camera (CA00), GPIO/LED (BA00), and Config (CF00) BLE services
 * on a single shared BLE server. Advertises as "ApiBLE".
 *
 * Target: nulllaborg ESP32-S3-CAM
 *   - 8 MB Flash, 8 MB OPI PSRAM
 *   - OV2640/OV3660 camera
 *   - Flash LED on GPIO 3
 *   - USB-C CDC serial
 *   - No onboard NeoPixel
 *   - No OLED
 *
 * Modules:
 *   - CameraBleModule:  CA00 service, FreeRTOS tasks, chunked JPEG delivery
 *   - GpioBleModule:    BA00 service, 5-pin GPIO + optional NeoPixel
 *   - ConfigBleModule:  CF00 service, extensible typed NVS config + Serial CLI
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include "ApiBleManager.h"
#include "CameraBleModule.h"
#include "GpioBleModule.h"
#include "ConfigBleModule.h"

ApiBleManager   mgr;
CameraBleModule cam;
GpioBleModule   gpio;
ConfigBleModule cfg;

void setup() {
    Serial.begin(921600);
    delay(500);

    mgr.begin();
    mgr.setMtuCallback([](uint16_t mtu) { cam.onMtuChanged(mtu); });

    cam.begin();
    cam.attach(mgr.server());

    gpio.begin();
    gpio.attach(mgr.server());

    cfg.begin("cfg");
    cfg.attach(mgr.server());

    mgr.startAdvertising();

    Serial.println();
    Serial.println("ApiBLE ready:");
    Serial.println("  Advertising as \"ApiBLE\"");
    Serial.println("  Camera  CA00  — snapshot, settings, chunked JPEG");
    Serial.println("  GPIO    BA00  — pin control, optional NeoPixel");
    Serial.println("  Config  CF00  — extensible typed NVS + Serial CLI");
    Serial.println();
    Serial.println("  Serial commands: name <str>, intv <n>, enab 0|1, gain <n.n>,");
    Serial.println("                   cfg key=val, show, list, clear");
    Serial.println();
}

void loop() {
    cfg.handleSerial();
    delay(100);
}
