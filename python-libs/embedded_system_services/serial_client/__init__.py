"""Serial Client — host-side serial client for ConfigStore firmware service.

Usage:
    from embedded_system_services.serial_client import ConfigStoreSerialClient

    client = ConfigStoreSerialClient()
    await client.connect()
    config = await client.get_config()
    await client.close()
"""
from embedded_system_services.serial_client.client import ConfigStoreSerialClient

__all__ = ["ConfigStoreSerialClient"]
