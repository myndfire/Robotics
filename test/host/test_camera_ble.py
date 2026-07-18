#!/usr/bin/env python3
"""CLI for testing CameraBluetoothServer firmware over BLE.

Communicates with an ESP32-S3 running CameraBluetoothServer
(firmware/apps/CameraBLE/) — advertises as "ESP32-CAM", delivers
chunked JPEG snapshots via the CA00 BLE service.

── Quick Start ───────────────────────────────────────────────────

    1. Flash the firmware:
       cd firmware/apps/CameraBLE
       uv run pio run --target upload

    2. Install host deps (from test/host/):
       cd test/host
       uv sync

    3. Run a test (from test/host/):
       uv run python test_camera_ble.py                     # single snapshot
       uv run python test_camera_ble.py --stream 5 --delay 2  # 5 captures
       uv run python test_camera_ble.py --settings           # read settings
       uv run python test_camera_ble.py --set quality=10     # change setting
       uv run python test_camera_ble.py --params             # valid settings
       uv run python test_camera_ble.py --flash              # flash photo
       uv run python test_camera_ble.py --reset              # factory reset

    Or from the repo root:
       uv run python test/host/test_camera_ble.py --stream 3

── BLE Protocol ───────────────────────────────────────────────────

    Service: 0000CA00-0000-1000-8000-00805F9B34FB
    CA01 (Control, Write):   "snapshot", "flash_on", "flash_off", "save", "reset"
    CA02 (Settings, Write+Read): "key=val,key=val,..."  partial updates
    CA03 (Frame, Notify):    Chunked JPEG (240-byte chunks, 4-byte LE header)
    CA04 (Info, Read):       Frame metadata JSON
    CA05 (Params, Read):     Valid settings schema JSON

── Settings Reference ─────────────────────────────────────────────

    quality    0-63       JPEG compression (0=best, 63=worst)
    brightness -2..2      Image brightness
    contrast   -2..2      Image contrast
    saturation -2..2      Color saturation (-2=grayscale)
    effect     0-6        0=off, 1=negative, 2=b&w, 3=red, 4=green, 5=blue, 6=sepia
    vflip      0/1        Vertical mirror
    hflip      0/1        Horizontal mirror
    wb         auto|sunny|cloudy|office|home  White balance preset
    aec        on|off     Auto-exposure control
    shutter    0-1200     Manual exposure (only when aec=off)
    gain       0-30       Manual analog gain (only when aec=off)

    Frame sizes: QVGA (320x240), VGA (640x480), SVGA (800x600),
    XGA (1024x768), HD (1280x720), SXGA (1280x1024), UXGA (1600x1200)

    Changing frame size triggers DMA pool reallocation (~50-200ms
    blocking).  All other settings apply instantly on next capture.
"""

import asyncio
import argparse
import sys

from embedded_system_services.camera_ble import CameraBleClient


async def main():
    parser = argparse.ArgumentParser(
        description="Test tool for CameraBluetoothServer via embedded_system_services"
    )
    parser.add_argument("--timeout", type=float, default=10.0,
                        help="BLE scan timeout in seconds (default: 10, increase if device is far)")
    parser.add_argument("--settings", action="store_true",
                        help="Read all camera settings from the ESP32 via CA02 characteristic (prints JSON)")
    parser.add_argument("--set", type=str, default=None, metavar="KEY=VAL,...",
                        help="Update camera settings. Comma-separated key=value pairs (partial updates)."
                             " Use --params to discover valid keys. Example: --set quality=10,brightness=1")
    parser.add_argument("--flash", action="store_true",
                        help="Take a photo with the flash LED enabled (turns LED on, captures, turns LED off)")
    parser.add_argument("--reset", action="store_true",
                        help="Factory reset — wipes ALL NVS (camera settings + WiFi credentials) and reboots the ESP32")
    parser.add_argument("--save", action="store_true",
                        help="Persist current camera settings to NVS flash (survives power cycles). Chain with --set.")
    parser.add_argument("--stream", type=int, default=0, metavar="N",
                        help="Capture N consecutive snapshots in sequence")
    parser.add_argument("--delay", type=float, default=1.0, metavar="SEC",
                        help="Delay between stream captures in seconds (default: 1s, use 0.5 for 2 fps)")
    parser.add_argument("--output", type=str, default="snapshot.jpg",
                        help="Output filename for single captures (default: snapshot.jpg)")
    parser.add_argument("--params", action="store_true",
                        help="Read valid settings schema from the ESP32 via CA05 characteristic (prints JSON)")
    args = parser.parse_args()

    client = CameraBleClient(timeout=args.timeout)

    try:
        await client.connect()

        # Settings update
        if args.set:
            settings = {}
            for pair in args.set.split(","):
                if "=" in pair:
                    k, v = pair.split("=", 1)
                    settings[k.strip()] = v.strip()
            await client.set_settings(**settings)

        # Persist
        if args.save:
            await client.save_settings()

        # Actions
        if args.params:
            params = await client.get_params()
            print(json.dumps(params, indent=2))
        elif args.settings:
            settings = await client.get_settings()
            print(json.dumps(settings, indent=2))
        elif args.reset:
            await client.reset()
        elif args.flash:
            await client.flash_capture(args.output)
        elif args.stream > 0:
            await client.stream(count=args.stream, delay_sec=args.delay)
        else:
            # Default: single snapshot
            await client.capture(args.output)

    except RuntimeError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        await client.disconnect()


if __name__ == "__main__":
    import json
    asyncio.run(main())
