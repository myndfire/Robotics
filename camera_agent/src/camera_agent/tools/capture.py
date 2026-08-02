"""BLE camera capture tool — ``take_picture``.

Wraps :class:`embedded_system_services.api_ble.ApiBleClient` with a
clean async interface and the shared ``@log_tool_call`` decorator.
"""

import os

from embedded_system_services.api_ble import ApiBleClient

from camera_agent.tools._debug import log_tool_call


@log_tool_call
async def take_picture(output_path: str = "snapshot.jpg") -> str:
    """Take a photo using the ESP32-S3 BLE camera.

    Connects to the ESP32-S3 running ApiBLE firmware
    (advertises as ApiBLE), captures a single JPEG snapshot, and
    saves it to disk.

    This tool does NOT describe the photo. To describe it, call
    ``describe_picture`` with the returned file path.

    Args:
        output_path: File path to save the JPEG image. Default: snapshot.jpg

    Returns:
        str: Confirmation message with file path and size in bytes.
             Example: "Photo saved to snapshot.jpg (4521 bytes)"
    """
    device = os.getenv("CAMERA_AGENT_DEVICE", "ApiBLE")

    client = ApiBleClient(device_name=device)
    try:
        await client.connect()
        info = await client.capture(output_path)
        size = info.get("size", "unknown")
        return f"Photo saved to {output_path} ({size} bytes)"
    finally:
        await client.disconnect()
