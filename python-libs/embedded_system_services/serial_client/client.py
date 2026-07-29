#!/usr/bin/env python3
"""Asyncio serial client for ConfigStore firmware service.

Port auto-detection matches PlatformIO logic:
  1. Enumerate serial ports
  2. Blacklist macOS noise (Bluetooth-Incoming-Port, wlan-debug)
  3. Prefer USB ports (vid is not None)
  4. Prefer description containing "UART"
  5. Pick last alphabetically
  6. If multiple candidates remain, fail with list
"""

import asyncio
import sys
from concurrent.futures import ThreadPoolExecutor

import serial
import serial.tools.list_ports


class SerialClient:
    """Base asyncio wrapper around pyserial."""

    def __init__(self, port=None, baud=921600, timeout=10.0):
        self._port_arg = port
        self._baud = baud
        self._timeout = timeout
        self._executor = ThreadPoolExecutor(max_workers=1)
        self._ser = None
        self._port = None

    # ── Port detection ──────────────────────────────────────

    def _find_port(self):
        if self._port_arg:
            return self._port_arg

        ports = list(serial.tools.list_ports.comports())

        # 1. Blacklist macOS noise
        filtered = []
        for p in ports:
            if sys.platform == "darwin" and p.device.endswith(
                ("Bluetooth-Incoming-Port", "wlan-debug")
            ):
                continue
            filtered.append(p)

        # 2. Prefer USB ports (vid is not None)
        usb_ports = [p for p in filtered if p.vid is not None]

        # 3. Prefer description containing "UART"
        uart_ports = [p for p in usb_ports if "UART" in (p.description or "")]

        candidates = uart_ports if uart_ports else usb_ports
        candidates = candidates if candidates else filtered

        if not candidates:
            raise RuntimeError("No serial ports found.")

        if len(candidates) > 1:
            # Sort alphabetically, pick last (matches PlatformIO fallback)
            candidates.sort(key=lambda p: p.device)

        # 4. If still ambiguous (>1), fail with list
        if len(candidates) > 1:
            msg = "Multiple serial ports found. Specify --port explicitly:\n"
            for p in candidates:
                msg += f"  {p.device} — {p.description or 'no desc'} (VID:PID={p.vid:04X}:{p.pid:04X})\n"
            raise RuntimeError(msg)

        return candidates[-1].device

    # ── Asyncio I/O wrappers ────────────────────────────────

    async def _write_line(self, line):
        loop = asyncio.get_event_loop()
        data = (line + "\n").encode()
        await loop.run_in_executor(self._executor, self._ser.write, data)

    async def _read_line(self):
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(self._executor, self._ser.readline)

    # ── Lifecycle ─────────────────────────────────────────

    async def connect(self):
        self._port = self._find_port()
        print(f"Opening {self._port} @ {self._baud} baud...")

        loop = asyncio.get_event_loop()
        self._ser = await loop.run_in_executor(
            self._executor,
            lambda: serial.Serial(
                self._port, self._baud, timeout=self._timeout
            ),
        )

        # Flush stale data
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()

        # Wait for device ready (look for "ConfigStore ready" or menu)
        deadline = asyncio.get_event_loop().time() + self._timeout
        while asyncio.get_event_loop().time() < deadline:
            line = await self._read_line()
            if b"ConfigStore ready" in line:
                print("Device ready.")
                return
            # Also consume menu output
        print("Connected (device may still be booting).")

    async def close(self):
        if self._ser:
            loop = asyncio.get_event_loop()
            await loop.run_in_executor(self._executor, self._ser.close)
            self._ser = None
            print("Closed.")

    async def __aenter__(self):
        await self.connect()
        return self

    async def __aexit__(self, exc_type, exc, tb):
        await self.close()


class ConfigStoreSerialClient(SerialClient):
    """High-level client for ConfigStore serial protocol."""

    # ── Commands ────────────────────────────────────────────

    async def _send_and_parse(self, cmd):
        await self._write_line(cmd)
        lines = []
        deadline = asyncio.get_event_loop().time() + 2.0
        while asyncio.get_event_loop().time() < deadline:
            raw = await self._read_line()
            if not raw:
                break
            line = raw.decode("utf-8", errors="replace").rstrip()
            lines.append(line)
            if "────" in line:
                break
        return lines

    async def get_config(self):
        lines = await self._send_and_parse("show")
        config = {}
        for line in lines:
            if "dev_name:" in line:
                config["dev_name"] = line.split(":", 1)[1].strip()
            elif "interval:" in line:
                config["interval"] = int(line.split(":", 1)[1].strip())
            elif "enabled:" in line:
                config["enabled"] = line.split(":", 1)[1].strip().lower() == "true"
            elif "gain:" in line:
                config["gain"] = float(line.split(":", 1)[1].strip())
            elif "Free NVS:" in line:
                config["free_entries"] = int(
                    line.split(":", 1)[1].strip().split()[0]
                )
        return config

    async def set_device_name(self, name):
        lines = await self._send_and_parse(f"name {name}")
        for line in lines:
            if "Dev name set:" in line:
                print(f"  Set name: {name}")
                return True
        raise RuntimeError(f"Failed to set device name: {lines}")

    async def set_interval(self, seconds):
        lines = await self._send_and_parse(f"intv {seconds}")
        for line in lines:
            if "Interval:" in line:
                print(f"  Set interval: {seconds}s")
                return True
        raise RuntimeError(f"Failed to set interval: {lines}")

    async def set_enabled(self, en):
        lines = await self._send_and_parse(f"enab {1 if en else 0}")
        for line in lines:
            if "Enabled:" in line:
                print(f"  Set enabled: {en}")
                return True
        raise RuntimeError(f"Failed to set enabled: {lines}")

    async def set_gain(self, value):
        lines = await self._send_and_parse(f"gain {value}")
        for line in lines:
            if "Gain:" in line:
                print(f"  Set gain: {value}")
                return True
        raise RuntimeError(f"Failed to set gain: {lines}")

    async def list_keys(self):
        lines = await self._send_and_parse("list")
        keys = []
        for line in lines:
            # Parse: "  dev_name      String  OK"
            parts = line.strip().split()
            if len(parts) >= 3 and parts[-1] in ("OK", "MISSING"):
                keys.append({
                    "key": parts[0],
                    "type": parts[1],
                    "present": parts[-1] == "OK",
                })
        return keys

    async def reset_to_defaults(self):
        lines = await self._send_and_parse("clear")
        for line in lines:
            if "Config reset to defaults" in line:
                print("  Reset to defaults.")
                return True
        raise RuntimeError(f"Failed to reset: {lines}")

    async def is_persisted(self):
        """Test NVS persistence across reboot."""
        print("Testing persistence...")
        await self.set_device_name("PersistTest")
        await self.set_interval(99)

        # Read back to confirm
        config = await self.get_config()
        assert config["dev_name"] == "PersistTest"
        assert config["interval"] == 99

        # Try DTR reboot
        await self.close()
        try:
            print("  Rebooting via DTR toggle...")
            with serial.Serial(self._port, self._baud, dsrdtr=True) as reboot_ser:
                reboot_ser.dtr = False
                await asyncio.sleep(0.1)
                reboot_ser.dtr = True
                await asyncio.sleep(0.1)
            await asyncio.sleep(3)  # Wait for boot
        except Exception as e:
            print(f"  Warning: DTR reboot failed ({e}). Skipping persistence test.")
            print("  Reconnecting manually...")
            await self.connect()
            return False

        await self.connect()
        config = await self.get_config()
        ok = config["dev_name"] == "PersistTest" and config["interval"] == 99
        print(f"  Persistence check: {'PASS' if ok else 'FAIL'}")
        return ok
