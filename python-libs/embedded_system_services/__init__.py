"""Host-side Python client library for ESP32-S3 embedded system services.

Subpackages:
    camera_ble   — BLE client for CameraBluetoothServer firmware service
    ble_api      — BLE client for BleApiServer firmware service
    serial_client — Serial client for ConfigStore firmware service
"""

from embedded_system_services.camera_ble import CameraBleClient
from embedded_system_services.ble_api import BleApiClient
from embedded_system_services.serial_client import ConfigStoreSerialClient

__all__ = ["CameraBleClient", "BleApiClient", "ConfigStoreSerialClient"]
