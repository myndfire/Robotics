/*
 * StorageController.cpp
 *
 * See StorageController.h for the public API description and usage examples.
 */

#include "StorageController.h"
#include <nvs_flash.h>

StorageController::StorageController()
    : _namespace("")
    , _open(false)
{}

void StorageController::begin(const char* ns) {
    // If already open with a different namespace, close first.
    if (_open) {
        end();
    }
    _namespace = ns;
    _prefs.begin(_namespace.c_str(), false);  // false = read-write
    _open = true;
}

void StorageController::end() {
    if (_open) {
        _prefs.end();
        _open = false;
    }
}

bool StorageController::remove(const char* key) {
    if (!_open) return false;
    return _prefs.remove(key);
}

bool StorageController::clear() {
    if (!_open) return false;
    return _prefs.clear();
}

void StorageController::eraseAll(bool skipRestart) {
    nvs_flash_erase();
    if (!skipRestart) {
        delay(500);
        ESP.restart();
    }
}

bool StorageController::exists(const char* key) {
    if (!_open) return false;
    return _prefs.isKey(key);
}

size_t StorageController::freeEntries() {
    if (!_open) return 0;
    return _prefs.freeEntries();
}

const char* StorageController::getNamespace() const {
    return _namespace.c_str();
}

bool StorageController::isOpen() const {
    return _open;
}
