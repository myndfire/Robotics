"""BLE camera capture tool — ``take_picture``.

Wraps :class:`embedded_system_services.api_ble.ApiBleClient` with a
clean async interface and the shared ``@log_tool_call`` decorator.

Captures are optimized for fast-moving objects by default: a short
exposure (``aec off`` + low ``shutter``) with raised ``gain`` to
freeze motion, plus BLE throughput knobs (``ble_mtu``,
``chunk_delay_ms``) to make the transfer quicker. All defaults may be
overridden per-call by the model or tuned globally via environment
variables (see ``camera_agent/README.md``).
"""

import os

from embedded_system_services.api_ble import ApiBleClient

from camera_agent.tools._debug import log_tool_call

DEFAULT_AEC = os.getenv("CAMERA_AGENT_AEC", "off")
DEFAULT_SHUTTER = int(os.getenv("CAMERA_AGENT_SHUTTER", "300"))
DEFAULT_GAIN = int(os.getenv("CAMERA_AGENT_GAIN", "20"))
DEFAULT_BLE_MTU = int(os.getenv("CAMERA_AGENT_BLE_MTU", "517"))
DEFAULT_CHUNK_DELAY_MS = int(os.getenv("CAMERA_AGENT_CHUNK_DELAY_MS", "2"))


@log_tool_call
async def take_picture(
    output_path: str = "snapshot.jpg",
    *,
    shutter: int = DEFAULT_SHUTTER,
    gain: int = DEFAULT_GAIN,
    aec: str = DEFAULT_AEC,
    ble_mtu: int = DEFAULT_BLE_MTU,
    chunk_delay_ms: int = DEFAULT_CHUNK_DELAY_MS,
    quality: int | None = None,
) -> str:
    """Take a photo using the ESP32-S3 BLE camera.

    Connects to the ESP32-S3 running ApiBLE firmware
    (advertises as ApiBLE), applies exposure settings for
    fast-moving objects, captures a single JPEG snapshot, and saves
    it to disk.

    By default a short exposure (``aec=off``, low ``shutter``) with
    higher ``gain`` is configured to freeze motion, along with
    throughput knobs (``ble_mtu``, ``chunk_delay_ms``) for a faster
    BLE transfer. These are persisted in firmware NVS, so they carry
    across captures. Defaults can be tuned via CAMERA_AGENT_* env
    vars and overridden per-call through the keyword args.

    This tool does NOT describe the photo. To describe it, call
    ``describe_picture`` with the returned file path.

    Args:
        output_path: File path to save the JPEG image. Default: snapshot.jpg
        shutter: Exposure value (lower = shorter exposure = less motion
            blur, but darker). Default: 300.
        gain: Sensor gain/ISO to brighten short exposures (higher =
            brighter but noisier). Default: 20.
        aec: Auto-exposure mode ("on"/"off"). Default: "off".
        ble_mtu: Local ATT MTU cap (23-517). Default: 517.
        chunk_delay_ms: Delay between BLE notify chunks. Lower = faster
            transfer. Default: 2.
        quality: JPEG quality (0-63). When None (default), the firmware's
            persisted value is left unchanged.

    Returns:
        str: Confirmation message with file path and size in bytes.
             Example: "Photo saved to snapshot.jpg (4521 bytes)"
    """
    device = os.getenv("CAMERA_AGENT_DEVICE", "ApiBLE")

    client = ApiBleClient(device_name=device)
    try:
        await client.connect()

        settings = {
            "aec": aec,
            "shutter": shutter,
            "gain": gain,
            "ble_mtu": ble_mtu,
            "chunk_delay_ms": chunk_delay_ms,
        }
        if quality is not None:
            settings["quality"] = quality
        await client.set_camera_settings(**settings)

        info = await client.capture(output_path)
        size = info.get("size", "unknown")
        return f"Photo saved to {output_path} ({size} bytes)"
    finally:
        await client.disconnect()
