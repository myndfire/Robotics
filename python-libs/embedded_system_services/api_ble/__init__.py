"""BLE client for the unified ApiBLE firmware service.

Usage:
    from embedded_system_services.api_ble import ApiBleClient

    client = ApiBleClient()
    await client.connect()
    await client.capture("photo.jpg")
    await client.set_pin(1, 1)
    await client.set_config(tenant_id="abc", user_id="xyz")
    await client.disconnect()
"""
from embedded_system_services.api_ble.client import ApiBleClient

__all__ = ["ApiBleClient"]
