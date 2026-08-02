#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>

/*
 * ApiBleManager — Shared BLE initialization and server lifecycle.
 *
 *   - BLEDevice::init("ApiBLE")
 *   - One BLEServer with connect/disconnect callbacks
 *   - Advertising start/restart on disconnect
 *
 * Modules register their own services on the shared server via attach().
 */

class ApiBleManager {
public:
    ApiBleManager();

    /* One-shot BLE stack init and server creation */
    void begin();

    /* Access the shared server for module attach() calls */
    BLEServer* server() const;

    /* Start advertising all registered services */
    void startAdvertising();

private:
    BLEServer* _server;
};
