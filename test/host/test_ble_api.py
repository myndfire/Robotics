#!/usr/bin/env python3
"""CLI for testing BleApiServer firmware over BLE.

Communicates with an ESP32-S3 running BleApiServer
(firmware/services/BleApiServer/ → firmware/apps/BleApiServerDemo/)
— advertises as "ESP32-API", exposes GPIO pins and NeoPixel LED
via the BA00 BLE service.

── Quick Start ─────────────────────────────────────────────────────

    1. Flash the firmware (choose your board):
       cd firmware/apps/BleApiServerDemo

       # ESP32-S3 DevKitC-1 (NeoPixel GPIO 48, NEO_GRB)
       uv run pio run -e esp32-s3-devkitc-1 --target upload

       # DORHEA Mini (NeoPixel GPIO 21, NEO_RGB)
       uv run pio run -e esp32-s3-dorhea --target upload

    2. Install host deps (from test/host/):
       cd test/host
       uv sync

    3. Run tests (from test/host/):
       uv run python test_ble_api.py                              # read state
       uv run python test_ble_api.py --config "pin=4:out"         # configure GPIO
       uv run python test_ble_api.py --config "pin=4:out:persist" # persist across reboot
       uv run python test_ble_api.py --config "pin=7:ain"         # analog input
       uv run python test_ble_api.py --set 4:1                    # GPIO HIGH
       uv run python test_ble_api.py --set 4:1:persist            # GPIO HIGH + persist
       uv run python test_ble_api.py --get 7                      # read GPIO 7
       uv run python test_ble_api.py --led red                    # set LED color

    Or from the repo root:
       uv run python test/host/test_ble_api.py --led green

── BLE Protocol ─────────────────────────────────────────────────────

    Service: 0000BA00-0000-1000-8000-00805F9B34FB
    BA01 (Control, Write):   key=val command pairs
    BA02 (Status, Read):     "led=R;G;B,pinN=val,..."

── Commands ─────────────────────────────────────────────────────────

    pin=<n>:<out|in|ain>[:persist]      Configure GPIO mode
    set=<n>:<0|1>[:persist]              Digital write
    get=<n>                              Read GPIO (digital or analog)
    led=<color>                          Set NeoPixel: red, green, blue, cyan,
                                         magenta, yellow, white, on, off
    onboard_RGBLed_pin=<n>               Remap NeoPixel GPIO at runtime
    onboard_RGBLed_type=<rgb|grb|gbr>    Remap colour byte order at runtime
"""

import asyncio
import argparse
import sys

from embedded_system_services.ble_api import BleApiClient
from embedded_system_services.ble_api.client import CONTROL_CHAR_UUID

VALID_LED = {"on", "off", "red", "green", "blue", "cyan", "magenta", "yellow", "white"}


async def main():
    parser = argparse.ArgumentParser(
        description="Test tool for BleApiServer via embedded_system_services"
    )
    parser.add_argument("--timeout", type=float, default=10.0,
                        help="BLE scan timeout in seconds (default: 10, increase if device is far)")
    parser.add_argument("--config", type=str, default=None,
                        help='GPIO pin setup. Format: "pin=4:out" or "pin=4:out:persist,pin=7:ain".'
                             ' Modes: out (digital output), in (digital input), ain (analog input).'
                             ' Add :persist suffix to save configuration to NVS.')
    parser.add_argument("--set", type=str, default=None,
                        help='Digital write. Format: "4:1" (HIGH) or "4:0" (LOW).'
                             ' Add :persist suffix to save the value to NVS.')
    parser.add_argument("--get", type=str, default=None,
                        help='Read a GPIO pin. Format: "7". Returns digital value unless pin'
                             ' was configured as analog (ain), in which case returns ADC reading.')
    parser.add_argument("--led", type=str, default=None,
                        help=f"Set NeoPixel color. Valid: {', '.join(sorted(VALID_LED))}")
    args = parser.parse_args()

    client = BleApiClient(timeout=args.timeout)

    try:
        await client.connect()

        if args.config:
            # Send raw config string to allow :persist suffix and onboard_RGBLed_* commands
            # that go beyond the BleApiClient convenience methods
            await client._client.write_gatt_char(
                CONTROL_CHAR_UUID,
                args.config.encode(), response=True
            )
            print(f"Config sent: {args.config}")
            await asyncio.sleep(0.2)

        if args.set:
            parts = args.set.split(":")
            if len(parts) == 2:
                await client.set_pin(int(parts[0]), int(parts[1]))

        if args.get:
            await client.get_pin(int(args.get))

        if args.led:
            await client.set_led(args.led)

        # Always read state after commands
        state = await client.read_state()
        print()
        print("── Device State ──")
        led_raw = state.get("led", "0;0;0")
        led_parts = led_raw.replace(";", ",").split(",")
        if len(led_parts) >= 3:
            print(f"  LED:   ({led_parts[0]}, {led_parts[1]}, {led_parts[2]})")
        pins = {k: v for k, v in state.items() if k.startswith("pin")}
        if pins:
            print(f"  Pins:")
            for k, v in sorted(pins.items(), key=lambda x: int(x[3:])):
                print(f"    GPIO {k[3:]}: {v}")
        else:
            print(f"  Pins: (none configured)")

    except RuntimeError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        await client.disconnect()


if __name__ == "__main__":
    asyncio.run(main())
