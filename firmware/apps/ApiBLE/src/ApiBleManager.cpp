#include "ApiBleManager.h"

ApiBleManager::ApiBleManager() : _server(nullptr) {}

void ApiBleManager::begin() {
    BLEDevice::init("ApiBLE");
    _server = BLEDevice::createServer();

    class SrvCB : public BLEServerCallbacks {
    public:
        void onConnect(BLEServer* s) override {
            (void)s;
            Serial.println("BLE: client connected");
        }
        void onDisconnect(BLEServer* s) override {
            (void)s;
            Serial.println("BLE: client disconnected, restarting advertising");
            s->getAdvertising()->start();
        }
    };
    _server->setCallbacks(new SrvCB());

    Serial.println("BLE: stack initialized");
}

BLEServer* ApiBleManager::server() const {
    return _server;
}

void ApiBleManager::startAdvertising() {
    if (_server) {
        _server->getAdvertising()->start();
        Serial.println("BLE: advertising started");
    }
}
