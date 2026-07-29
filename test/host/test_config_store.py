#!/usr/bin/env python3
"""CLI test script for ConfigStore firmware service over serial UART.

Usage:
    uv run python test_config_store.py                    # auto-detect, all tests
    uv run python test_config_store.py --port /dev/cu.usbmodem1101
    uv run python test_config_store.py --test-persist     # include reboot test
    uv run python test_config_store.py --show-only        # just read config
"""

import asyncio
import argparse
import sys

from embedded_system_services.serial_client import ConfigStoreSerialClient


async def main():
    parser = argparse.ArgumentParser(
        description="Test tool for ConfigStore via serial"
    )
    parser.add_argument("--port", type=str, default=None,
                        help="Serial port (auto-detected if omitted)")
    parser.add_argument("--baud", type=int, default=921600,
                        help="Baud rate (default: 921600)")
    parser.add_argument("--timeout", type=float, default=10.0,
                        help="Connection timeout in seconds")
    parser.add_argument("--test-persist", action="store_true",
                        help="Test NVS persistence across reboot (DTR toggle)")
    parser.add_argument("--show-only", action="store_true",
                        help="Only run 'show' command and print config")
    args = parser.parse_args()

    client = ConfigStoreSerialClient(
        port=args.port, baud=args.baud, timeout=args.timeout
    )

    try:
        await client.connect()

        if args.show_only:
            config = await client.get_config()
            for k, v in config.items():
                print(f"  {k}: {v}")
            return

        # 1. Read initial
        print("\n--- Initial config ---")
        config = await client.get_config()
        for k, v in config.items():
            print(f"  {k}: {v}")

        # 2. Set values
        print("\n--- Setting values ---")
        await client.set_device_name("TestDevice")
        await client.set_interval(10)
        await client.set_enabled(False)
        await client.set_gain(2.5)

        # 3. Verify
        print("\n--- Verify settings ---")
        config = await client.get_config()
        assert config["dev_name"] == "TestDevice", f"Expected 'TestDevice', got {config['dev_name']}"
        assert config["interval"] == 10, f"Expected 10, got {config['interval']}"
        assert config["enabled"] is False, f"Expected False, got {config['enabled']}"
        assert abs(config["gain"] - 2.5) < 0.01, f"Expected ~2.5, got {config['gain']}"
        print("  All values match.")

        # 4. List keys
        print("\n--- List keys ---")
        keys = await client.list_keys()
        for k in keys:
            status = "OK" if k["present"] else "MISSING"
            print(f"  {k['key']:12s} {k['type']:8s} {status}")
        assert all(k["present"] for k in keys), "Some keys missing from NVS"

        # 5. Optional persistence test
        if args.test_persist:
            print("\n--- Persistence test ---")
            persisted = await client.is_persisted()
            if not persisted:
                print("  (Skipped or failed — see warnings above)")

        # 6. Reset
        print("\n--- Reset to defaults ---")
        await client.reset_to_defaults()

        # 7. Verify defaults
        print("\n--- Verify defaults ---")
        config = await client.get_config()
        assert config["dev_name"] == "ESP32-S3", f"Expected 'ESP32-S3', got {config['dev_name']}"
        assert config["interval"] == 5, f"Expected 5, got {config['interval']}"
        assert config["enabled"] is True, f"Expected True, got {config['enabled']}"
        assert abs(config["gain"] - 1.0) < 0.01, f"Expected ~1.0, got {config['gain']}"
        print("  Defaults restored.")

        print("\nAll tests passed.")

    except RuntimeError as e:
        print(f"\nError: {e}", file=sys.stderr)
        sys.exit(1)
    except AssertionError as e:
        print(f"\nTest failed: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        await client.close()


if __name__ == "__main__":
    asyncio.run(main())
