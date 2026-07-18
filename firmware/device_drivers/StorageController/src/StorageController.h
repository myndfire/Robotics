/*
 * StorageController.h — Full CRUD key-value store over ESP32 NVS (Preferences)
 *
 * Owns a named NVS namespace and provides type-safe Create/Read/Update/Delete
 * operations, key existence checking, namespace clearing, free space query,
 * and factory reset (erase all NVS partitions).
 *
 * Unlike the lightweight Storage (Library/Storage/), StorageController:
 *   - Binds a namespace once in begin() — no need to pass "moisture" to
 *     every get()/put() call.
 *   - Supports remove() for single-key deletion.
 *   - Supports clear() to delete all keys in the namespace.
 *   - Supports eraseAll() for full factory reset (wipes ALL NVS partitions).
 *   - Supports exists() to check whether a key is present.
 *   - Supports freeEntries() to monitor remaining NVS capacity.
 *
 * Supported types (via explicit template specialisations):
 *   int, float, unsigned long, bool, String
 *
 * ==================== Usage Example ====================
 *
 *   #include <StorageController.h>
 *
 *   StorageController store;
 *
 *   void setup() {
 *     store.begin("config");                     // bind namespace
 *
 *     store.put<int>("brightness", 75);          // Create
 *     int b = store.get<int>("brightness", 50);  // Read (returns 75)
 *
 *     store.put<int>("brightness", 100);         // Update
 *
 *     store.remove("brightness");                 // Delete
 *     bool hasKey = store.exists("brightness");   // false
 *
 *     Serial.printf("Free NVS entries: %d\n", store.freeEntries());
 *
 *     // store.eraseAll();  // factory reset — wipes ALL NVS namespaces!
 *     store.end();
 *   }
 *
 * ==================== NVS Limits ====================
 * ESP32-S3 default NVS partition is ~12–24 KB depending on partition table.
 * Each key-value entry consumes ~40+ bytes (key + value + overhead).
 * Use freeEntries() to avoid running out of space at runtime.
 */

#pragma once

#include <Arduino.h>
#include <Preferences.h>

class StorageController {
public:
    /*
     * Constructor.
     *
     * Does not open any namespace — call begin() before using get/put/etc.
     */
    StorageController();

    /*
     * Open a namespace for read/write access.
     *
     * ns — namespace identifier.  Must be 1–15 characters, unique within
     *      the NVS partition.  Multiple StorageController instances can
     *      use different namespaces simultaneously.  Opening a second
     *      namespace without calling end() on the first is supported by
     *      the underlying Preferences library but NOT by this wrapper.
     *
     * NVS namespace names are not hierarchical — "moisture" and
     * "moisture_cal" are separate, independent namespaces.
     */
    void begin(const char* ns);

    /*
     * Close the current namespace.
     *
     * After calling end(), all CRUD operations are no-ops until begin()
     * is called again.  Calling end() on an already-closed controller
     * is safe (no-op).
     */
    void end();

    // ── CRUD Operations ───────────────────────────────────────────────

    /*
     * Write a key-value pair to the namespace.  Creates the key if it
     * does not exist; overwrites the value if it does.
     *
     * key   — key name, 1–15 characters.
     * value — value to store.
     *
     * No-op if end() has been called or no namespace is open.
     */
    template<typename T>
    void put(const char* key, T value);

    /*
     * Read a key from the namespace.
     *
     * key        — key name.
     * defaultVal — value returned if the key does not exist.
     *
     * Returns the stored value, or defaultVal if the key is missing or
     * the namespace is not open.
     */
    template<typename T>
    T get(const char* key, T defaultVal);

    /*
     * Delete a single key from the namespace.
     *
     * Returns true if the key was removed.  Returns false if the key
     * did not exist or the namespace is not open.
     *
     * Deleting a non-existent key is safe (Preferences::remove returns
     * false).  Does NOT free the underlying NVS page — the space is
     * reclaimed when the page is garbage-collected on next boot.
     */
    bool remove(const char* key);

    /*
     * Delete ALL keys in the current namespace.
     *
     * Returns true on success.  After a successful clear(), get() will
     * return defaultVal for all previously-stored keys.  The namespace
     * itself remains open and usable for new put() calls.
     */
    bool clear();

    /*
     * Factory reset — erases ALL NVS partitions.
     *
     * WARNING: This destroys every namespace on the ESP32, including
     * WiFi credentials stored by WiFiManager, calibration data stored
     * by MoistureSensorController, and any other NVS data.  After
     * calling eraseAll(), the ESP32 MUST be restarted for changes to
     * take effect.  This function calls ESP.restart() internally
     * unless skipRestart is true.
     *
     * skipRestart — if true, only erases NVS without rebooting.
     *               Caller is responsible for restarting.
     */
    void eraseAll(bool skipRestart = false);

    // ── Query Methods ─────────────────────────────────────────────────

    /*
     * Check whether a key exists in the current namespace.
     *
     * Returns true if the key is present (regardless of its value).
     * Returns false if the key does not exist or the namespace is not
     * open.
     *
     * Usage:
     *   if (store.exists("calibrated")) {
     *     // load stored calibration
     *   }
     */
    bool exists(const char* key);

    /*
     * Return the number of free NVS entries available for the current
     * namespace.
     *
     * Each put() consumes at least one entry.  When freeEntries()
     * approaches 0, consider calling clear() to reclaim space.
     *
     * Returns 0 if the namespace is not open.
     */
    size_t freeEntries();

    // ── State Getters ─────────────────────────────────────────────────

    /*
     * Return the current namespace name, or an empty string if begin()
     * has not been called.
     */
    const char* getNamespace() const;

    /*
     * Returns true if a namespace is currently open and ready for CRUD
     * operations.
     */
    bool isOpen() const;

private:
    Preferences _prefs;
    String      _namespace;
    bool        _open;
};

// ── Type specialisations (inline) ────────────────────────────────────────

template<> inline void StorageController::put<int>(const char* key, int value) {
    if (_open) _prefs.putInt(key, value);
}

template<> inline int StorageController::get<int>(const char* key, int defaultVal) {
    if (!_open) return defaultVal;
    return _prefs.getInt(key, defaultVal);
}

template<> inline void StorageController::put<float>(const char* key, float value) {
    if (_open) _prefs.putFloat(key, value);
}

template<> inline float StorageController::get<float>(const char* key, float defaultVal) {
    if (!_open) return defaultVal;
    return _prefs.getFloat(key, defaultVal);
}

template<> inline void StorageController::put<unsigned long>(const char* key, unsigned long value) {
    if (_open) _prefs.putULong(key, value);
}

template<> inline unsigned long StorageController::get<unsigned long>(const char* key, unsigned long defaultVal) {
    if (!_open) return defaultVal;
    return _prefs.getULong(key, defaultVal);
}

template<> inline void StorageController::put<bool>(const char* key, bool value) {
    if (_open) _prefs.putBool(key, value);
}

template<> inline bool StorageController::get<bool>(const char* key, bool defaultVal) {
    if (!_open) return defaultVal;
    return _prefs.getBool(key, defaultVal);
}

template<> inline void StorageController::put<String>(const char* key, String value) {
    if (_open) _prefs.putString(key, value);
}

template<> inline String StorageController::get<String>(const char* key, String defaultVal) {
    if (!_open) return defaultVal;
    return _prefs.getString(key, defaultVal);
}
