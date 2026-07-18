# BleApiServer

BLE peripheral server exposing GPIO control and NeoPixel LED control over BLE GATT characteristics. Advertises as "ESP32-API".

## Dependencies

| Library | Source |
|---|---|
| Adafruit NeoPixel | External (`adafruit/Adafruit NeoPixel`) |

## Setup

### Build and Flash

```bash
cd services/BleApiServer

# ESP32-S3 DevKitC-1 (default)
uv run pio run -e esp32-s3-devkitc-1 --target upload

# DORHEA Mini
uv run pio run -e esp32-s3-dorhea --target upload
```

External project:

```ini
lib_extra_dirs =
    /path/to/Robotics/firmware/device_drivers
    /path/to/Robotics/firmware/services
```

## API

```cpp
#include <BleApiServer.h>

// Default: NeoPixel on GPIO 48, NEO_GRB+NEO_KHZ800
BleApiServer server;

// Custom: NeoPixel on GPIO 21, NEO_RGB+NEO_KHZ800 (DORHEA Mini)
BleApiServer server(21, NEO_RGB + NEO_KHZ800);

void setup() {
    Serial.begin(921600);
    server.begin();  // Initializes BLE + NeoPixel, starts advertising
}
```

### Methods

| Method | Description |
|---|---|
| `begin()` | Initialize NeoPixel, create BLE server, start advertising as "ESP32-API" |
| `setLedColor(color)` | Set NeoPixel: "red", "green", "blue", "cyan", "magenta", "yellow", "white", "on", "off" |
| `configurePin(pin, mode)` | Configure GPIO: "out", "in", "ain" |
| `setPin(pin, value)` | digitalWrite GPIO (HIGH/LOW) |
| `readPin(pin)` | digitalRead GPIO |

## BLE Protocol

### Service: `0000BA00-0000-1000-8000-00805F9B34FB`

| Characteristic | UUID | Properties | Purpose |
|---|---|---|---|
| Control | BA01 | Write | Key=value command pairs |
| Status | BA02 | Read | JSON state response |

### Commands (write to BA01)

| Format | Example | Action |
|---|---|---|
| `led=<color>` | `led=red` | Set NeoPixel color |
| `pin=<n>:<mode>` | `pin=4:out` | Configure GPIO |
| `set=<n>:<0\|1>` | `set=4:1` | Digital write |

Multiple commands in one write: `led=green,pin=4:out,set=4:1`

## Python Test Client

```bash
uv run python test_api_ble.py                     # read state
uv run python test_api_ble.py --led red            # set LED
uv run python test_api_ble.py --config "pin=4:out" # configure GPIO
uv run python test_api_ble.py --set 4:1            # GPIO HIGH
```

## Board Variants

| Environment | Board | NeoPixel GPIO | Color Order |
|---|---|---|---|
| `esp32-s3-devkitc-1` | DevKitC-1 | 48 | NEO_GRB |
| `esp32-s3-dorhea` | DORHEA Mini | 21 | NEO_RGB |
