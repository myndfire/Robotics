#include "ApiBleManager.h"

ApiBleManager::ApiBleManager() : _server(nullptr) {}

void ApiBleManager::begin() {
    BLEDevice::init("ApiBLE");
    _server = BLEDevice::createServer();

    class SrvCB : public BLEServerCallbacks {
    public:
        SrvCB(ApiBleManager* m) : _m(m) {}
        void onConnect(BLEServer* s) override {
            (void)s;
            Serial.println("BLE: client connected");
        }
        void onDisconnect(BLEServer* s) override {
            (void)s;
            Serial.println("BLE: client disconnected, restarting advertising");
            s->getAdvertising()->start();
        }
        void onMtuChanged(BLEServer* s, esp_ble_gatts_cb_param_t* param) override {
            (void)s;
            uint16_t mtu = param->mtu.mtu;
            Serial.printf("BLE: negotiated MTU = %u\n", mtu);
            if (_m->_mtuCallback) _m->_mtuCallback(mtu);
        }
    private:
        ApiBleManager* _m;
    };
    _server->setCallbacks(new SrvCB(this));

    // Local MTU cap negotiated with each host (macOS/WinRT auto-negotiate up to this)
    BLEDevice::setMTU(_localMtu);

    Serial.println("BLE: stack initialized");
}

void ApiBleManager::setLocalMtu(uint16_t mtu) {
    if (mtu < 23) mtu = 23;
    if (mtu > 517) mtu = 517;
    _localMtu = mtu;
    BLEDevice::setMTU(_localMtu);
}

void ApiBleManager::setMtuCallback(std::function<void(uint16_t)> cb) {
    _mtuCallback = std::move(cb);
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
