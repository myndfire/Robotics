"""BLE Camera Client — host-side client for CameraBluetoothServer firmware service.

Usage:
    from embedded_system_services.camera_ble import CameraBleClient

    client = CameraBleClient()
    await client.connect()
    await client.capture("photo.jpg")
    settings = await client.get_settings()
    await client.disconnect()
"""
from embedded_system_services.camera_ble.client import CameraBleClient

__all__ = ["CameraBleClient"]
