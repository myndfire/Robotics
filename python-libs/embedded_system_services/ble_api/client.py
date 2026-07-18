"""
BleApiClient — BLE client for the BleApiServer firmware service.

Communicates with an ESP32-S3 running BleApiServer over BLE.
Provides GPIO configuration, digital write/read, and NeoPixel LED control.

Protocol:
    Service UUID:     0000BA00-0000-1000-8000-00805F9B34FB
    Control (BA01):   Write   — key=val command pairs
    Status (BA02):    Read    — full state string (led=R,G,B,pin4=1,pin7=0,...)
"""

import asyncio
import time

try:
    from bleak import BleakScanner, BleakClient
except ImportError:
    BleakScanner = None
    BleakClient = None

API_SERVICE_UUID  = "0000ba00-0000-1000-8000-00805f9b34fb"
CONTROL_CHAR_UUID = "0000ba01-0000-1000-8000-00805f9b34fb"
STATUS_CHAR_UUID  = "0000ba02-0000-1000-8000-00805f9b34fb"

VALID_LED_COLORS = {"on", "off", "red", "green", "blue", "cyan", "magenta", "yellow", "white"}


class BleApiClient:
    """BLE client for ESP32-S3 BleApiServer (GPIO + LED control)."""

    def __init__(self, device_name="ESP32-API", timeout=10.0):
        self._device_name = device_name
        self._timeout = timeout
        self._client = None
        self._device = None

    # ── Connection ──────────────────────────────────────────────

    async def connect(self, name=None):
        """Scan for the BLE device, connect, and discover characteristics.

        Args:
            name: Override device name to scan for (default: "ESP32-API").

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

        print(f"Connected: {self._client.is_connected}")
        return True

    async def disconnect(self):
        """Disconnect from the BLE device."""
        if self._client and self._client.is_connected:
            await self._client.disconnect()
            print("Disconnected.")

    # ── State ───────────────────────────────────────────────────

    async def read_state(self) -> dict:
        """Read the full device state from the Status characteristic.

        Returns:
            dict with keys like 'led' (R;G;B string) and 'pinN' (value).
        """
        data = await self._client.read_gatt_char(STATUS_CHAR_UUID)
        raw = bytes(data).decode()
        parts = {}
        for item in raw.split(","):
            if "=" in item:
                k, v = item.split("=", 1)
                parts[k] = v
        return parts

    # ── GPIO ────────────────────────────────────────────────────

    async def configure_pin(self, pin, mode="out"):
        """Configure a GPIO pin mode on the ESP32.

        Sends "pin=<n>:<mode>" to the Control characteristic.

        Args:
            pin: GPIO pin number.
            mode: "out" (output, LOW by default), "in" (digital input),
                  or "ain" (analog input).
        """
        payload = f"pin={pin}:{mode}"
        await self._client.write_gatt_char(
            CONTROL_CHAR_UUID, payload.encode(), response=True
        )
        print(f"Config sent: {payload}")

    async def set_pin(self, pin, value):
        """Digital write to a GPIO pin.

        Sends "set=<n>:<0|1>" to the Control characteristic.

        Args:
            pin: GPIO pin number (must be configured as output first).
            value: 0 for LOW, 1 for HIGH.
        """
        payload = f"set={pin}:{value}"
        await self._client.write_gatt_char(
            CONTROL_CHAR_UUID, payload.encode(), response=True
        )
        print(f"Sent: '{payload}'")

    async def get_pin(self, pin) -> str:
        """Read a GPIO pin value.

        Sends "get=<n>" to Control, then reads Status for the result.

        Args:
            pin: GPIO pin number.

        Returns:
            String value of the pin reading (e.g., "1" or "0").
        """
        payload = f"get={pin}"
        await self._client.write_gatt_char(
            CONTROL_CHAR_UUID, payload.encode(), response=True
        )
        await asyncio.sleep(0.05)
        state = await self.read_state()
        pin_key = f"pin{pin}"
        val = state.get(pin_key, "N/A")
        print(f"GPIO {pin}: {val}")
        return val

    # ── LED ─────────────────────────────────────────────────────

    async def set_led(self, color):
        """Set the NeoPixel LED color on the ESP32.

        Sends "led=<color>" to the Control characteristic.

        Args:
            color: One of "red", "green", "blue", "cyan", "magenta",
                   "yellow", "white", "on" (alias for green), "off".

        Raises:
            ValueError: If color is not a valid LED color.
        """
        color = color.lower()
        if color not in VALID_LED_COLORS:
            raise ValueError(
                f"Invalid LED color '{color}'. Valid: {', '.join(sorted(VALID_LED_COLORS))}"
            )
        payload = f"led={color}"
        await self._client.write_gatt_char(
            CONTROL_CHAR_UUID, payload.encode(), response=True
        )
        print(f"Sent: '{payload}'")

    # ── Convenience ─────────────────────────────────────────────

    async def configure_pins(self, **pins):
        """Configure multiple GPIO pins at once.

        Args:
            **pins: Key-value pairs where key is pin number (int) and
                    value is mode string. Example: pin4="out", pin7="in".
        """
        parts = [f"pin={pin}:{mode}" for pin, mode in pins.items()]
        payload = ",".join(parts)
        await self._client.write_gatt_char(
            CONTROL_CHAR_UUID, payload.encode(), response=True
        )
        print(f"Multi-config sent: {payload}")
