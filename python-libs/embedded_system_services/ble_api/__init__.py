"""BLE API Client — host-side client for BleApiServer firmware service.

Usage:
    from embedded_system_services.ble_api import BleApiClient

    client = BleApiClient()
    await client.connect()
    await client.configure_pin(4, "out")
    await client.set_pin(4, 1)
    await client.set_led("green")
    await client.disconnect()
"""
from embedded_system_services.ble_api.client import BleApiClient

__all__ = ["BleApiClient"]
