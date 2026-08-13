#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <functional>

/*
 * ApiBleManager — Shared BLE initialization and server lifecycle.
 *
 *   - BLEDevice::init("ApiBLE")
 *   - One BLEServer with connect/disconnect callbacks
 *   - Local MTU cap + negotiated-MTU change notification
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

    /* Set the local ATT MTU cap (23..517) before advertising */
    void setLocalMtu(uint16_t mtu);

    /* Register a callback invoked with the negotiated peer MTU on change */
    void setMtuCallback(std::function<void(uint16_t)> cb);

    /* Start advertising all registered services */
    void startAdvertising();

private:
    BLEServer* _server;
    uint16_t   _localMtu{517};
    std::function<void(uint16_t)> _mtuCallback;
};
