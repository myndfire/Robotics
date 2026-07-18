"""
CameraBleClient — BLE client for the CameraBluetoothServer firmware service.

Communicates with an ESP32-S3 running CameraBluetoothServer over BLE.
Provides snapshot capture, settings management, streaming, flash photography,
parameter discovery, and factory reset.

Protocol:
    Service UUID:     0000CA00-0000-1000-8000-00805F9B34FB
    Control (CA01):   Write   — "snapshot", "flash_on", "flash_off", "save", "reset"
    Settings (CA02):  Write+Read — "key=val,key=val,..." partial updates
    Frame (CA03):     Notify  — chunked JPEG (240-byte chunks)
    Info (CA04):      Read    — JSON: {"size":N,"w":W,"h":H,"q":Q}
    Params (CA05):    Read    — JSON schema of valid settings keys/ranges

    Frame chunking protocol:
        Notification 1: [4 bytes total_size LE] + first ≤240 bytes of JPEG
        Notifications 2+: ≤240 bytes each continuation data
        Client accumulates until total_size bytes received.
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

SERVICE_UUID      = "0000ca00-0000-1000-8000-00805f9b34fb"
CONTROL_CHAR_UUID = "0000ca01-0000-1000-8000-00805f9b34fb"
SETTINGS_CHAR_UUID = "0000ca02-0000-1000-8000-00805f9b34fb"
FRAME_CHAR_UUID   = "0000ca03-0000-1000-8000-00805f9b34fb"
INFO_CHAR_UUID    = "0000ca04-0000-1000-8000-00805f9b34fb"
PARAMS_CHAR_UUID  = "0000ca05-0000-1000-8000-00805f9b34fb"


class CameraBleClient:
    """BLE client for ESP32-S3 CameraBluetoothServer."""

    def __init__(self, device_name="ESP32-CAM", timeout=10.0, chunk_timeout=5.0):
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
        """Scan for the BLE device, connect, and discover characteristics.

        Args:
            name: Override device name to scan for (default: "ESP32-CAM").

        Returns:
            True if connected successfully.

        Raises:
            RuntimeError: If device not found or bleak not installed.
        """
        if BleakScanner is None:
            raise RuntimeError("Bleak is required: pip install embedded-system-services")

        search_name = name or self._device_name
        print(f"Scanning for '{search_name}' (timeout {self._timeout}s)...")

        self._device = None

        def callback(d, adv_data):
            nonlocal self
            if d.name and search_name in d.name:
                self._device = d

        async with BleakScanner(callback) as scanner:
            start = time.monotonic()
            while self._device is None and (time.monotonic() - start) < self._timeout:
                await asyncio.sleep(0.2)

        if self._device is None:
            raise RuntimeError(
                f"Device '{search_name}' not found. Is the board powered and advertising?"
            )

        print(f"Found: {self._device.name} ({self._device.address})")

        self._client = BleakClient(self._device.address)
        await self._client.connect()

        if not self._client.is_connected:
            raise RuntimeError("Failed to connect")

        # Subscribe to frame notifications for chunked JPEG delivery
        self._frame_chunks = []
        self._frame_total = 0
        self._frame_event = asyncio.Event()

        await self._client.start_notify(FRAME_CHAR_UUID, self._on_frame_notify)

        print(f"Connected: {self._client.is_connected}")
        return True

    async def disconnect(self):
        """Disconnect from the BLE device."""
        if self._client and self._client.is_connected:
            if self._frame_event:
                try:
                    await self._client.stop_notify(FRAME_CHAR_UUID)
                except Exception:
                    pass
            await self._client.disconnect()
            print("Disconnected.")

    # ── Frame chunk reception ───────────────────────────────────

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
        """Wait for a complete frame to arrive, return assembled bytes."""
        self._frame_total = 0
        self._frame_chunks = []
        self._frame_event.clear()

        await self._client.write_gatt_char(
            CONTROL_CHAR_UUID, b"snapshot", response=True
        )

        try:
            await asyncio.wait_for(self._frame_event.wait(), timeout=self._chunk_timeout)
        except asyncio.TimeoutError:
            raise RuntimeError("Timed out waiting for frame chunks")

        data = b"".join(self._frame_chunks)
        return data

    # ── Capture ─────────────────────────────────────────────────

    async def capture(self, output_path="snapshot.jpg"):
        """Take a single photo and save to disk.

        Sends "snapshot" over BLE → ESP32 captures a JPEG frame →
        delivers in 240-byte chunks → client reassembles and saves.

        Args:
            output_path: File path to save the JPEG image.

        Returns:
            dict with keys: size (bytes), width, height, quality.
        """
        data = await self._wait_for_frame()

        with open(output_path, "wb") as f:
            f.write(data)

        size = len(data)
        print(f"Frame saved: {output_path} ({size} bytes)")

        info = {}
        try:
            raw = await self._client.read_gatt_char(INFO_CHAR_UUID)
            info = json.loads(bytes(raw).decode())
            print(f"  Info: {info.get('w', '?')}x{info.get('h', '?')}, "
                  f"quality={info.get('q', '?')}")
        except Exception:
            pass

        return info

    async def stream(self, count=5, delay_sec=1.0, output_pattern="snapshot_{:02d}.jpg"):
        """Capture multiple consecutive photos.

        Args:
            count: Number of frames to capture.
            delay_sec: Delay between captures in seconds.
            output_pattern: Filename pattern (use {:02d} for zero-padded index).

        Returns:
            List of dicts with frame info for each capture.
        """
        results = []
        for i in range(count):
            print(f"\nCapture {i+1}/{count}:")
            path = output_pattern.format(i + 1)
            info = await self.capture(path)
            results.append(info)
            if i < count - 1:
                await asyncio.sleep(delay_sec)
        return results

    # ── Settings ────────────────────────────────────────────────

    async def get_settings(self) -> dict:
        """Read current camera settings from the ESP32.

        Reads the CA02 Settings characteristic — returns all camera settings
        (frame size, JPEG quality, brightness, contrast, saturation,
        special effect, vflip/hflip, white balance, AEC, shutter, gain).

        Returns:
            dict of setting key → value. Empty dict on error.
        """
        try:
            raw = await self._client.read_gatt_char(SETTINGS_CHAR_UUID)
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

    async def set_settings(self, **settings):
        """Update camera settings.

        Writes key=value pairs to the CA02 Settings characteristic.
        Only specified keys are changed — others keep current values.
        Settings take effect on the next capture.
        To persist changes across reboots, call save_settings() after.

        Args:
            **settings: Key-value pairs to set.
                        Example: quality=10, framesize=6, vflip=0
        """
        payload = ",".join(f"{k}={v}" for k, v in settings.items())
        await self._client.write_gatt_char(
            SETTINGS_CHAR_UUID, payload.encode(), response=True
        )
        print(f"Settings sent: {payload}")

    async def save_settings(self):
        """Persist current camera settings to NVS flash.

        Sends "save" to the Control characteristic. Settings survive
        power cycles after this call.
        """
        await self._client.write_gatt_char(
            CONTROL_CHAR_UUID, b"save", response=True
        )
        print("Settings saved to NVS.")

    # ── Params ──────────────────────────────────────────────────

    async def get_params(self) -> dict:
        """Get valid setting keys, types, and ranges from the ESP32.

        Reads the CA05 Params characteristic — returns a JSON schema
        of all valid setting keys and their allowed values.

        Returns:
            dict — JSON schema. Empty dict on error.
        """
        try:
            raw = await self._client.read_gatt_char(PARAMS_CHAR_UUID)
            return json.loads(bytes(raw).decode())
        except Exception as e:
            print(f"Error reading params: {e}")
            return {}

    # ── Flash ───────────────────────────────────────────────────

    async def flash_capture(self, output_path="snapshot.jpg"):
        """Take a photo with the flash LED enabled.

        Turns on the LED, captures a frame, turns off the LED.
        Saved to output_path.

        Args:
            output_path: File path to save the JPEG image.
        """
        await self._client.write_gatt_char(
            CONTROL_CHAR_UUID, b"flash_on", response=True
        )
        await asyncio.sleep(0.1)
        data = await self._wait_for_frame()
        await self._client.write_gatt_char(
            CONTROL_CHAR_UUID, b"flash_off", response=True
        )

        with open(output_path, "wb") as f:
            f.write(data)

        print(f"Flash photo saved: {output_path} ({len(data)} bytes)")

    # ── Reset ───────────────────────────────────────────────────

    async def reset(self):
        """Factory reset — wipe all NVS and reboot the ESP32.

        Sends "reset" to the Control characteristic. Destroys ALL NVS
        namespaces including camera settings and WiFi credentials.
        The ESP32 reboots automatically.
        """
        await self._client.write_gatt_char(
            CONTROL_CHAR_UUID, b"reset", response=True
        )
        print("Factory reset sent — board is rebooting.")
