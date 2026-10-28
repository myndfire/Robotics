# firmware

ESP32-S3 firmware for the Robotics project. Organized as reusable driver and service libraries plus flashable applications (PlatformIO projects).

See [`ARCHITECTURE.md`](ARCHITECTURE.md) for the platform layers, app/service composition, and a deep dive into the ApiBLE reference app (BLE services, camera pipeline, GPIO, configuration, and concurrency).

```
firmware/
├── apps/            # Flashable PlatformIO apps (main.cpp + platformio.ini)
├── services/        # Service libraries (library.json) — compose drivers & logic
└── device_drivers/  # Hardware driver libraries (library.json)
```

## Layout

| Directory | Kind | Imported via | Flashable? |
|---|---|---|---|
| [`apps/`](apps/) | Applications | — | Yes (upload to ESP32-S3) |
| [`services/`](services/) | Service libraries | `lib_extra_dirs` | No |
| [`device_drivers/`](device_drivers/) | Hardware driver libraries | `lib_extra_dirs` | No |

Apps compose services; services compose device drivers. Apps declare their dependencies through PlatformIO `lib_extra_dirs`:

```ini
lib_extra_dirs =
    ../services          # firmware/services/
    ../device_drivers    # firmware/device_drivers/
```

## Build & Flash

Install PlatformIO once:

```bash
uv tool install platformio==6.1.19
```

Every app is a standalone PlatformIO project. From the repo root:

```bash
cd firmware/apps/<App>
pio run --target upload             # build + flash
pio run --target upload --target monitor  # flash + open serial monitor
```

## Targets

All firmware targets an **ESP32-S3** (nulllaborg ESP32-S3 CAM for camera apps, DevKitC-1 for sensor/demo apps). See each app's `platformio.ini` for the exact board.

## ApiBLE

The primary unified BLE app is [`apps/ApiBLE`](apps/ApiBLE/). It combines Camera (CA00), GPIO (BA00), and Config (CF00) BLE services on a single server, advertising as **"ApiBLE"**, and exposes persisted over-the-air settings — including BLE throughput knobs (`ble_mtu`, `chunk_size`, `chunk_delay_ms`) and capture controls (`shutter`, `gain`, `aec`, `size`, `quality`).

ApiBLE is self-contained: its Camera/GPIO/Config modules live in `apps/ApiBLE/src`, and it does **not** link any library from `services/`.

| App | Services used |
|---|---|
| `ApiBLE` | none (app-local BLE modules) |
| `CameraWebServerDemo` | `CameraWebServer` |
| `MoistureMonitorDemo` | `MoistureMonitor` |
| `TimedCycleControllerDemo` | `TimedCycleController` |
| `StatusDashboardDemo` | `StatusDashboard` |
| `MoisturePump` | `MoistureMonitor`, `TimedCycleController` |
| `StatusPanel` | `StatusDashboard` |
| `MultiSensor` | none (device drivers only) |

See:

- [test/host README](../test/host/README.md) — flashing, CLI usage, BLE protocol, throughput tuning, and capturing fast-moving objects
- [python-libs README](../python-libs/README.md) — host-side `ApiBleClient` library
- [root README](../README.md) — project overview
