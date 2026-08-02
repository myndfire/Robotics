"""Host-side Python client library for ESP32-S3 embedded system services.

Subpackage:
    api_ble  — Unified BLE client for ApiBLE firmware (Camera + GPIO + Config)
"""

from embedded_system_services.api_ble import ApiBleClient

__all__ = ["ApiBleClient"]
