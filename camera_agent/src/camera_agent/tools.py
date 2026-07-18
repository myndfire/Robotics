"""Camera tool for the managed agent — captures photos via BLE from ESP32-S3."""

import os
from embedded_system_services.camera_ble import CameraBleClient


async def take_picture(output_path: str = "snapshot.jpg") -> str:
    """Take a photo using the ESP32-S3 BLE camera.

    Connects to the ESP32-S3 running CameraBluetoothServer firmware
    (advertises as ESP32-CAM), captures a single JPEG snapshot, and
    saves it to disk.

    Args:
        output_path: File path to save the JPEG image. Default: snapshot.jpg

    Returns:
        str: Confirmation message with file path and size in bytes.
             Example: "Photo saved to snapshot.jpg (4521 bytes)"
    """
    device = os.getenv("CAMERA_AGENT_DEVICE", "ESP32-CAM")

    client = CameraBleClient(device_name=device)
    try:
        await client.connect()
        info = await client.capture(output_path)
        size = info.get("size", "unknown")
        return f"Photo saved to {output_path} ({size} bytes)"
    finally:
        await client.disconnect()
