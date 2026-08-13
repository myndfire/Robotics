"""
ApiBleClient — Unified BLE client for the ApiBLE firmware app.

Communicates with an ESP32-S3 running the ApiBLE app over BLE.
Provides unified access to all three services:
  - Camera  (CA00) — snapshots, settings, chunked JPEG
  - GPIO    (BA00) — pin control, optional NeoPixel LED
  - Config  (CF00) — extensible typed NVS configuration

Protocol:
    Service UUIDs:
      Camera  CA00: 0000CA00-0000-1000-8000-00805F9B34FB
      GPIO    BA00: 0000BA00-0000-1000-8000-00805F9B34FB
      Config  CF00: 0000CF00-0000-1000-8000-00805F9B34FB

    Camera characteristics:
      CA01 Write  — "snapshot", "flash_on", "flash_off"
      CA02 Write+Read — settings string
      CA03 Notify — chunked JPEG (chunk_size-byte chunks, 4-byte LE header; auto-sized from negotiated MTU)
      CA04 Read   — frame info JSON
      CA05 Read   — settings params schema

    GPIO characteristics:
      BA01 Write  — "pin=1:out", "set=1:1", "get=1", "led=red"
      BA02 Read   — status string

    Config characteristics:
      CF01 Write  — "key=val,key=val,..."
      CF02 Read   — full state string
      CF03 Read   — schema string
"""

import asyncio
import json
import struct
import time

try:
    from bleak import BleakScanner, BleakClient
except ImportError:
    BleakScanner = None
    BleakClient = None

# ── Service UUIDs ──────────────────────────────────────────────────────────

CAMERA_SERVICE_UUID   = "0000ca00-0000-1000-8000-00805f9b34fb"
GPIO_SERVICE_UUID     = "0000ba00-0000-1000-8000-00805f9b34fb"
CONFIG_SERVICE_UUID   = "0000cf00-0000-1000-8000-00805f9b34fb"

# Camera
CA_CONTROL_CHAR   = "0000ca01-0000-1000-8000-00805f9b34fb"
CA_SETTINGS_CHAR  = "0000ca02-0000-1000-8000-00805f9b34fb"
CA_FRAME_CHAR     = "0000ca03-0000-1000-8000-00805f9b34fb"
CA_INFO_CHAR      = "0000ca04-0000-1000-8000-00805f9b34fb"
CA_PARAMS_CHAR    = "0000ca05-0000-1000-8000-00805f9b34fb"

# GPIO
BA_CONTROL_CHAR   = "0000ba01-0000-1000-8000-00805f9b34fb"
BA_STATUS_CHAR    = "0000ba02-0000-1000-8000-00805f9b34fb"

# Config
CF_CONTROL_CHAR   = "0000cf01-0000-1000-8000-00805f9b34fb"
CF_STATE_CHAR     = "0000cf02-0000-1000-8000-00805f9b34fb"
CF_SCHEMA_CHAR    = "0000cf03-0000-1000-8000-00805f9b34fb"

VALID_LED_COLORS = {"on", "off", "red", "green", "blue", "cyan", "magenta", "yellow", "white"}


class ApiBleClient:
    """Unified BLE client for ApiBLE firmware (Camera + GPIO + Config)."""

    def __init__(self, device_name="ApiBLE", timeout=10.0, chunk_timeout=5.0):
        self._device_name = device_name
        self._timeout = timeout
        self._chunk_timeout = chunk_timeout
        self._client = None
        self._device = None
        self._frame_chunks = []
        self._frame_total = 0
        self._frame_event = None

    # ── Connection ──────────────────────────────────────────────

    async def connect(self, name=None):
        """Scan for the BLE device and connect."""
        if BleakScanner is None:
            raise RuntimeError("Bleak is required: pip install bleak")

        search_name = name or self._device_name
        print(f"Scanning for '{search_name}' (timeout {self._timeout}s)...")

        self._device = None

        def callback(d, adv_data):
            nonlocal self
            advertised = getattr(adv_data, "local_name", None) or ""
            if (d.name and search_name in d.name) or (advertised and search_name in advertised):
                self._device = d

        async with BleakScanner(callback) as scanner:
            start = time.monotonic()
            while self._device is None and (time.monotonic() - start) < self._timeout:
                await asyncio.sleep(0.2)

        if self._device is None:
            raise RuntimeError(f"Device '{search_name}' not found. Is it powered and advertising?")

        print(f"Found: {self._device.name} ({self._device.address})")

        self._client = BleakClient(self._device.address)
        await self._client.connect()

        if not self._client.is_connected:
            raise RuntimeError("Failed to connect")

        # Subscribe to frame notifications
        self._frame_event = asyncio.Event()
        await self._client.start_notify(CA_FRAME_CHAR, self._on_frame_notify)

        try:
            mtu = getattr(self._client, "mtu_size", 0) or 0
            print(f"Connected: {self._client.is_connected} (ATT MTU: {mtu})")
        except Exception:
            print(f"Connected: {self._client.is_connected}")
        return True

    async def disconnect(self):
        """Disconnect from the BLE device."""
        if self._client and self._client.is_connected:
            try:
                await self._client.stop_notify(CA_FRAME_CHAR)
            except Exception:
                pass
            await self._client.disconnect()
            print("Disconnected.")

    # ── Camera: Frame chunk reception ───────────────────────────

    def _on_frame_notify(self, sender, data):
        """Reassemble chunked JPEG frames from BLE notifications."""
        if self._frame_total == 0:
            if len(data) < 4:
                return
            self._frame_total = struct.unpack("<I", data[:4])[0]
            self._frame_chunks = [data[4:]]
        else:
            self._frame_chunks.append(data)

        total = sum(len(c) for c in self._frame_chunks)
        if total >= self._frame_total:
            self._frame_event.set()

    async def _wait_for_frame(self) -> bytes:
        """Wait for a complete frame, return assembled bytes."""
        self._frame_total = 0
        self._frame_chunks = []
        self._frame_event.clear()

        await self._client.write_gatt_char(
            CA_CONTROL_CHAR, b"snapshot", response=True
        )

        try:
            await asyncio.wait_for(self._frame_event.wait(), timeout=self._chunk_timeout)
        except asyncio.TimeoutError:
            raise RuntimeError("Timed out waiting for frame chunks")

        return b"".join(self._frame_chunks)

    # ── Camera: Capture ───────────────────────────────────────

    async def capture(self, output_path="snapshot.jpg"):
        """Take a single photo and save to disk."""
        data = await self._wait_for_frame()

        with open(output_path, "wb") as f:
            f.write(data)

        size = len(data)
        print(f"Frame saved: {output_path} ({size} bytes)")

        info = {}
        try:
            raw = await self._client.read_gatt_char(CA_INFO_CHAR)
            info = json.loads(bytes(raw).decode())
            print(f"  Info: {info.get('w', '?')}x{info.get('h', '?')}, quality={info.get('q', '?')}")
        except Exception:
            pass

        return info

    async def stream(self, count=5, delay_sec=1.0, output_pattern="snapshot_{:02d}.jpg"):
        """Capture multiple consecutive photos."""
        results = []
        for i in range(count):
            print(f"\nCapture {i+1}/{count}:")
            path = output_pattern.format(i + 1)
            info = await self.capture(path)
            results.append(info)
            if i < count - 1:
                await asyncio.sleep(delay_sec)
        return results

    async def flash_capture(self, output_path="snapshot.jpg"):
        """Take a photo with flash LED enabled."""
        await self._client.write_gatt_char(CA_CONTROL_CHAR, b"flash_on", response=True)
        await asyncio.sleep(0.1)
        data = await self._wait_for_frame()
        await self._client.write_gatt_char(CA_CONTROL_CHAR, b"flash_off", response=True)

        with open(output_path, "wb") as f:
            f.write(data)

        print(f"Flash photo saved: {output_path} ({len(data)} bytes)")

    # ── Camera: Settings ──────────────────────────────────────

    async def get_camera_settings(self) -> dict:
        """Read current camera settings."""
        try:
            raw = await self._client.read_gatt_char(CA_SETTINGS_CHAR)
            text = bytes(raw).decode()
            settings = {}
            for pair in text.split(","):
                if "=" in pair:
                    k, v = pair.split("=", 1)
                    settings[k.strip()] = v.strip()
            return settings
        except Exception as e:
            print(f"Error reading settings: {e}")
            return {}

    async def set_camera_settings(self, **settings):
        """Update camera settings."""
        payload = ",".join(f"{k}={v}" for k, v in settings.items())
        await self._client.write_gatt_char(
            CA_SETTINGS_CHAR, payload.encode(), response=True
        )
        print(f"Camera settings sent: {payload}")

    async def get_camera_params(self) -> dict:
        """Get valid camera setting keys, types, and ranges."""
        try:
            raw = await self._client.read_gatt_char(CA_PARAMS_CHAR)
            return json.loads(bytes(raw).decode())
        except Exception as e:
            print(f"Error reading params: {e}")
            return {}

    # ── GPIO ────────────────────────────────────────────────────

    async def configure_pin(self, pin, mode="out"):
        """Configure a GPIO pin mode."""
        payload = f"pin={pin}:{mode}"
        await self._client.write_gatt_char(
            BA_CONTROL_CHAR, payload.encode(), response=True
        )
        print(f"GPIO config sent: {payload}")

    async def set_pin(self, pin, value):
        """Digital write to a GPIO pin."""
        payload = f"set={pin}:{value}"
        await self._client.write_gatt_char(
            BA_CONTROL_CHAR, payload.encode(), response=True
        )
        print(f"GPIO write sent: {payload}")

    async def get_pin(self, pin) -> str:
        """Read a GPIO pin value."""
        payload = f"get={pin}"
        await self._client.write_gatt_char(
            BA_CONTROL_CHAR, payload.encode(), response=True
        )
        await asyncio.sleep(0.05)
        state = await self.read_gpio_state()
        pin_key = f"pin{pin}"
        val = state.get(pin_key, "N/A")
        print(f"GPIO {pin}: {val}")
        return val

    async def read_gpio_state(self) -> dict:
        """Read full GPIO/LED state."""
        data = await self._client.read_gatt_char(BA_STATUS_CHAR)
        raw = bytes(data).decode()
        parts = {}
        for item in raw.split(","):
            if "=" in item:
                k, v = item.split("=", 1)
                parts[k] = v
        return parts

    async def set_led(self, color):
        """Set NeoPixel LED color (if present)."""
        color = color.lower()
        if color not in VALID_LED_COLORS:
            raise ValueError(f"Invalid LED color '{color}'. Valid: {', '.join(sorted(VALID_LED_COLORS))}")
        payload = f"led={color}"
        await self._client.write_gatt_char(
            BA_CONTROL_CHAR, payload.encode(), response=True
        )
        print(f"LED sent: {payload}")

    # ── Config ──────────────────────────────────────────────────

    async def get_config(self) -> dict:
        """Read full config state."""
        try:
            raw = await self._client.read_gatt_char(CF_STATE_CHAR)
            text = bytes(raw).decode()
            config = {}
            for pair in text.split(","):
                if "=" in pair:
                    k, v = pair.split("=", 1)
                    config[k.strip()] = v.strip()
            return config
        except Exception as e:
            print(f"Error reading config: {e}")
            return {}

    async def set_config(self, **kwargs):
        """Set one or more config keys."""
        payload = ",".join(f"{k}={v}" for k, v in kwargs.items())
        await self._client.write_gatt_char(
            CF_CONTROL_CHAR, payload.encode(), response=True
        )
        print(f"Config sent: {payload}")

    async def get_config_schema(self) -> str:
        """Read config schema (key:type:default)."""
        try:
            raw = await self._client.read_gatt_char(CF_SCHEMA_CHAR)
            return bytes(raw).decode()
        except Exception as e:
            print(f"Error reading schema: {e}")
            return ""

    async def reset_config(self):
        """Clear all config keys."""
        await self._client.write_gatt_char(
            CF_CONTROL_CHAR, b"__clear", response=True
        )
        print("Config cleared.")
