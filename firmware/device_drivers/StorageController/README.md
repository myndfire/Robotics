# StorageController

Full CRUD key-value store over ESP32 NVS (Non-Volatile Storage) using the Arduino `Preferences` library. Provides type-safe Create/Read/Update/Delete operations, key existence checking, namespace clearing, free space query, and factory reset capabilities.

## What it contains

| File | Role |
|---|---|
| `src/StorageController.h` | Class declaration — template put/get for 6 types, CRUD operations, namespace management, template specializations |
| `src/StorageController.cpp` | Implementation — Preferences wrapper, namespace open/close, eraseAll, query methods |
| `library.json` | PlatformIO library manifest |

## Setup

### PlatformIO

```ini
lib_extra_dirs = /path/to/Robotics/firmware/device_drivers
```

### Dependencies

| Dependency | Source |
|---|---|
| `Preferences.h` | Bundled with Arduino-ESP32 framework |
| `nvs_flash.h` | Bundled with ESP-IDF (used for `eraseAll`) |

No external libraries needed.

## Configuration

### Constructor

No constructor parameters. Create an instance and call `begin()` to open a namespace.

### Namespace Management

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `begin(ns)` | `const char*` | 1 – 15 characters | _required_ | Open a namespace for read/write access. The namespace name must be unique within the NVS partition. Namespace names are **not hierarchical** — `"moisture"` and `"moisture_cal"` are separate, independent namespaces. Multiple `StorageController` instances can use different namespaces simultaneously, but a single instance should only have one namespace open at a time. Call `end()` before opening a different namespace on the same instance. |
| `end()` | — | — | — | Close the current namespace. After calling `end()`, all CRUD operations become no-ops. Safe to call on an already-closed controller. |
| `getNamespace()` | `const char*` | — | — | Returns the current namespace name as a C string, or an empty string if `begin()` has not been called. |
| `isOpen()` | `bool` | — | — | Returns `true` if a namespace is currently open and ready for CRUD operations. |

### Supported Types

The following types are supported via template specializations for `put()` and `get()`:

| Type | `put()` method | `get()` parameters | Description |
|---|---|---|---|
| `int` | `put<int>(key, value)` | `get<int>(key, defaultVal)` | 32-bit signed integer. Range: -2,147,483,648 to 2,147,483,647. |
| `float` | `put<float>(key, value)` | `get<float>(key, defaultVal)` | 32-bit IEEE 754 floating point. ~7 significant digits. |
| `unsigned long` | `put<unsigned long>(key, value)` | `get<unsigned long>(key, defaultVal)` | 32-bit unsigned integer. Range: 0 to 4,294,967,295. |
| `bool` | `put<bool>(key, value)` | `get<bool>(key, defaultVal)` | Boolean true/false. Stored as 1 byte. |
| `String` | `put<String>(key, value)` | `get<String>(key, defaultVal)` | Arduino String. Variable-length, stored as length-prefixed data. Be mindful of NVS space with long strings. |

### Key Constraints

- Key names must be **1–15 characters** (same as Preferences library limit).
- Keys are case-sensitive: `"DryRef"` and `"dryref"` are different keys.
- Key names should use only alphanumeric characters and underscores for best compatibility.
- A single namespace can hold many keys — limited only by NVS partition size.

### CRUD Operations

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `put<T>(key, value)` | `template<T>` | Key: 1–15 chars | — | Write a key-value pair. Creates the key if it doesn't exist; overwrites the value if it does. No-op if namespace is not open. Each `put()` consumes at least one NVS entry. |
| `get<T>(key, defaultVal)` | `template<T>` | Key: 1–15 chars | User-specified default | Read a key. Returns the stored value, or `defaultVal` if the key does not exist or the namespace is not open. |
| `remove(key)` | `const char*` | Key: 1–15 chars | — | Delete a single key. Returns `true` if removed, `false` if key didn't exist or namespace closed. **Does NOT immediately free NVS space** — the underlying flash page is garbage-collected on next boot. Safe to call on non-existent keys. |
| `clear()` | — | — | — | Delete **ALL keys** in the current namespace. Returns `true` on success. After `clear()`, all `get()` calls will return their `defaultVal`. The namespace remains open and ready for new `put()` calls. |

### Factory Reset

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `eraseAll(skipRestart)` | `bool` | true / false | false | **Wipes ALL NVS partitions on the ESP32.** This destroys every namespace including WiFi credentials, calibration data, and any other persisted state. If `skipRestart` is false (default), calls `ESP.restart()` internally after erasing. If `skipRestart` is true, only erases NVS without rebooting — caller is responsible for restarting. |

### Query Methods

| Method | Returns | Description |
|---|---|---|
| `exists(key)` | `bool` | Returns `true` if the key exists in the current namespace (regardless of its value). Returns `false` if missing or namespace closed. Useful for checking whether calibration data has been stored yet. |
| `freeEntries()` | `size_t` | Returns the number of free NVS entries available in the current namespace. When approaching 0, consider calling `clear()` to reclaim space. Returns 0 if namespace is not open. Use this to monitor remaining capacity at runtime. |

## Usage

```cpp
#include <StorageController.h>

StorageController store;

void setup() {
    Serial.begin(921600);
    store.begin("config");  // Open "config" namespace

    // Create/Update
    store.put<int>("brightness", 75);
    store.put<bool>("enabled", true);
    store.put<String>("name", "ESP32-S3");

    // Read
    int b = store.get<int>("brightness", 50);     // returns 75
    bool e = store.get<bool>("enabled", false);    // returns true
    String n = store.get<String>("name", "N/A");  // returns "ESP32-S3"

    // Default if key doesn't exist
    int x = store.get<int>("nonexistent", 100);   // returns 100

    // Check existence
    if (store.exists("brightness")) {
        Serial.println("Brightness is set");
    }

    // Delete single key
    store.remove("brightness");

    // Clear all keys in namespace
    store.clear();

    // Monitor capacity
    Serial.printf("Free entries: %d\n", store.freeEntries());

    // Factory reset (wipes everything, then reboots)
    // store.eraseAll();  // CAREFUL: wipes WiFi credentials too!

    store.end();  // Close namespace
}

void loop() { }
```

## NVS Limits

| Parameter | Value | Description |
|---|---|---|
| Partition size | ~12–24 KB | Default NVS partition on ESP32-S3, depending on partition table |
| Entry overhead | ~40+ bytes per key | Each key-value pair consumes ~40 bytes + value size + overhead |
| String storage | ~40 + length bytes | Variable-length strings consume 40 overhead + string length |
| Max keys per namespace | ~200–500 | Practical limit at 12 KB partition with small values |
| Namespace limit | 1–15 chars | Namespace name length (NVS restriction) |
| Key name limit | 1–15 chars | Key name length (Preferences library restriction) |

## Hardware

No external hardware needed. Uses ESP32's internal flash memory NVS partition.

## Expected Behavior

- `begin("namespace")` opens the namespace. If the namespace doesn't exist, it's created on first `put()`.
- **No-op on closed namespace**: If `end()` has been called, all `put()`, `get()`, `remove()`, `clear()`, `exists()`, and `freeEntries()` calls do nothing (return default values, 0, or false).
- **Persistence**: Values survive power cycles, resets, and firmware updates (unless the partition table changes). NVS data is stored in the `nvs` partition of the flash.
- `put()` overwrites silently — no error if the key already exists.
- `get()` with a non-existent key returns the `defaultVal` provided. Does not create the key.
- `remove()` on a non-existent key returns `false` — no error, no crash.
- `clear()` deletes all keys but keeps the namespace open. Subsequent `put()` calls work normally.
- **`freeEntries()` example**: A freshly-booted ESP32-S3 with no stored data typically shows ~300–600 free entries depending on partition size. After storing ~10 keys, expect ~250–570 remaining.
- **`eraseAll()` warning**: This is destructive to ALL namespaces. WiFi credentials (stored by WiFiManager in a separate namespace) will be wiped. The ESP32 reboots automatically unless `skipRestart=true`. After reboot, WiFi must be reconfigured.
- **Garbage collection**: When `remove()` or `clear()` is called, freed space is NOT immediately available. NVS marks pages for GC, which runs on next boot. `freeEntries()` reflects currently available entries, not including space that will be reclaimed after reboot.
