#!/usr/bin/env python3
"""CLI test tool for the unified ApiBLE firmware over BLE.

Communicates with an ESP32-S3 running ApiBLE — advertises as "ApiBLE",
delivers camera snapshots, GPIO control, and extensible typed config
via three BLE services (CA00, BA00, CF00).

── Quick Start ───────────────────────────────────────────────────

    1. Flash the firmware:
       cd firmware/apps/ApiBLE
       uv run pio run --target upload

    2. Install host deps:
       cd test/host
       uv sync

    3. Run tests:
       uv run python test_api_ble.py                     # single snapshot
       uv run python test_api_ble.py --stream 5 --delay 2  # 5 captures
       uv run python test_api_ble.py --camera-settings     # read settings
       uv run python test_api_ble.py --set-cam quality=10  # change setting
       uv run python test_api_ble.py --pin 1:out           # configure GPIO
       uv run python test_api_ble.py --set-pin 1:1         # GPIO HIGH
       uv run python test_api_ble.py --get-pin 14          # read GPIO 14
       uv run python test_api_ble.py --config tenant_id=abc,user_id=xyz
       uv run python test_api_ble.py --show-config         # read all config
       uv run python test_api_ble.py --schema              # read config schema

── BLE Protocol ───────────────────────────────────────────────────

    Camera  CA00:
      CA01 Write  — "snapshot", "flash_on", "flash_off"
      CA02 Write+Read — settings string
      CA03 Notify — chunked JPEG (240-byte chunks, 4-byte LE header)
      CA04 Read   — frame info JSON
      CA05 Read   — params schema JSON

    GPIO    BA00:
      BA01 Write  — "pin=1:out", "set=1:1", "get=1", "led=red"
      BA02 Read   — status string

    Config  CF00:
      CF01 Write  — "key=val,key=val,..."
      CF02 Read   — full state string
      CF03 Read   — schema string
"""

import asyncio
import argparse
import json
import sys

from embedded_system_services.api_ble import ApiBleClient


async def main():
    parser = argparse.ArgumentParser(
        description="Test tool for ApiBLE unified firmware via embedded_system_services"
    )
    parser.add_argument("--timeout", type=float, default=10.0,
                        help="BLE scan timeout in seconds (default: 10)")

    # Camera
    camera = parser.add_argument_group("Camera (CA00)")
    camera.add_argument("--snapshot", type=str, default=None, metavar="FILE",
                        help="Take a single snapshot (default: snapshot.jpg)")
    camera.add_argument("--stream", type=int, default=0, metavar="N",
                        help="Capture N consecutive snapshots")
    camera.add_argument("--delay", type=float, default=1.0, metavar="SEC",
                        help="Delay between stream captures (default: 1s)")
    camera.add_argument("--flash", action="store_true",
                        help="Take a photo with flash LED enabled")
    camera.add_argument("--camera-settings", action="store_true",
                        help="Read camera settings")
    camera.add_argument("--set-cam", type=str, default=None, metavar="KEY=VAL,...",
                        help="Update camera settings (e.g., quality=10,vflip=0)")
    camera.add_argument("--cam-params", action="store_true",
                        help="Read valid camera settings schema")

    # GPIO
    gpio = parser.add_argument_group("GPIO (BA00)")
    gpio.add_argument("--pin", type=str, default=None,
                      help='Configure pin mode. Format: "1:out" or "14:ain"')
    gpio.add_argument("--set-pin", type=str, default=None,
                      help='Digital write. Format: "1:1" (HIGH) or "1:0" (LOW)')
    gpio.add_argument("--get-pin", type=str, default=None,
                      help='Read pin. Format: "14"')
    gpio.add_argument("--led", type=str, default=None,
                      help="Set LED color (red, green, blue, cyan, magenta, yellow, white, on, off)")
    gpio.add_argument("--gpio-state", action="store_true",
                      help="Read full GPIO/LED state")

    # Config
    config = parser.add_argument_group("Config (CF00)")
    config.add_argument("--config", type=str, default=None, metavar="KEY=VAL,...",
                        help="Set config key(s). Example: tenant_id=abc,user_id=xyz")
    config.add_argument("--show-config", action="store_true",
                        help="Read all stored config keys")
    config.add_argument("--schema", action="store_true",
                        help="Read config schema (key:type:default)")
    config.add_argument("--clear-config", action="store_true",
                        help="Clear all config keys")

    args = parser.parse_args()

    # Default action: snapshot if nothing else specified
    if not any([
        args.snapshot is not None, args.stream > 0, args.flash,
        args.camera_settings, args.set_cam, args.cam_params,
        args.pin, args.set_pin, args.get_pin, args.led, args.gpio_state,
        args.config, args.show_config, args.schema, args.clear_config
    ]):
        args.snapshot = "snapshot.jpg"

    client = ApiBleClient(timeout=args.timeout)

    try:
        await client.connect()

        # ── Camera ──────────────────────────────────────────────
        if args.set_cam:
            settings = {}
            for pair in args.set_cam.split(","):
                if "=" in pair:
                    k, v = pair.split("=", 1)
                    settings[k.strip()] = v.strip()
            await client.set_camera_settings(**settings)

        if args.camera_settings:
            settings = await client.get_camera_settings()
            print(json.dumps(settings, indent=2))

        if args.cam_params:
            params = await client.get_camera_params()
            print(json.dumps(params, indent=2))

        if args.flash:
            await client.flash_capture(args.snapshot or "snapshot_flash.jpg")
        elif args.stream > 0:
            await client.stream(count=args.stream, delay_sec=args.delay)
        elif args.snapshot:
            await client.capture(args.snapshot)

        # ── GPIO ────────────────────────────────────────────────
        if args.pin:
            parts = args.pin.split(":")
            if len(parts) == 2:
                await client.configure_pin(int(parts[0]), parts[1])
            else:
                print("Invalid pin format. Use: --pin 1:out")

        if args.set_pin:
            parts = args.set_pin.split(":")
            if len(parts) == 2:
                await client.set_pin(int(parts[0]), int(parts[1]))
            else:
                print("Invalid set-pin format. Use: --set-pin 1:1")

        if args.get_pin:
            await client.get_pin(int(args.get_pin))

        if args.led:
            await client.set_led(args.led)

        if args.gpio_state:
            state = await client.read_gpio_state()
            print("── GPIO State ──")
            for k, v in sorted(state.items()):
                print(f"  {k}: {v}")

        # ── Config ───────────────────────────────────────────────
        if args.config:
            cfg = {}
            for pair in args.config.split(","):
                if "=" in pair:
                    k, v = pair.split("=", 1)
                    cfg[k.strip()] = v.strip()
            await client.set_config(**cfg)

        if args.show_config:
            config = await client.get_config()
            print("── Config ──")
            for k, v in sorted(config.items()):
                print(f"  {k}: {v}")

        if args.schema:
            schema = await client.get_config_schema()
            print("── Config Schema ──")
            for entry in schema.split(","):
                print(f"  {entry}")

        if args.clear_config:
            await client.reset_config()

    except RuntimeError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        await client.disconnect()


if __name__ == "__main__":
    asyncio.run(main())
