"""BLE camera capture tools — ``take_picture`` and ``take_picture_with_flash``.

Both tools wrap :class:`embedded_system_services.api_ble.ApiBleClient`.
Normal captures use the firmware's plain snapshot command. Flash captures
use the firmware's atomic ``flash_snapshot`` command, which owns flash
enable, exposure settling, frame copy, and flash disable.

The tools are deliberately separate: the model must choose the flash tool
only when the user explicitly asks for flash.
"""

import os
from contextlib import asynccontextmanager

from embedded_system_services.api_ble import ApiBleClient

from camera_agent.tools._debug import log_tool_call

DEFAULT_AEC = os.getenv("CAMERA_AGENT_AEC", "off")
DEFAULT_SHUTTER = int(os.getenv("CAMERA_AGENT_SHUTTER", "300"))
DEFAULT_GAIN = int(os.getenv("CAMERA_AGENT_GAIN", "20"))
DEFAULT_BLE_MTU = int(os.getenv("CAMERA_AGENT_BLE_MTU", "517"))
DEFAULT_CHUNK_DELAY_MS = int(os.getenv("CAMERA_AGENT_CHUNK_DELAY_MS", "2"))


def _build_settings(
    *,
    shutter: int,
    gain: int,
    aec: str,
    ble_mtu: int,
    chunk_delay_ms: int,
    quality: int | None,
) -> dict:
    """Build one CA02 settings payload."""
    settings = {
        "aec": aec,
        "shutter": shutter,
        "gain": gain,
        "ble_mtu": ble_mtu,
        "chunk_delay_ms": chunk_delay_ms,
    }
    if quality is not None:
        settings["quality"] = quality
    return settings


@asynccontextmanager
async def _camera_session(device: str):
    """Open one BLE connection and guarantee disconnection."""
    client = ApiBleClient(device_name=device)
    await client.connect()
    try:
        yield client
    finally:
        await client.disconnect()


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
    """Take a normal photo using the ESP32-S3 BLE camera.

    This tool never uses the flash. Use ``take_picture_with_flash`` only
    when the user explicitly asks for flash.

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
    settings = _build_settings(
        shutter=shutter,
        gain=gain,
        aec=aec,
        ble_mtu=ble_mtu,
        chunk_delay_ms=chunk_delay_ms,
        quality=quality,
    )

    async with _camera_session(device) as client:
        await client.set_camera_settings(**settings)
        info = await client.capture(output_path)

    size = info.get("size", "unknown")
    return f"Photo saved to {output_path} ({size} bytes)"


@log_tool_call
async def take_picture_with_flash(
    output_path: str = "snapshot.jpg",
    *,
    shutter: int = DEFAULT_SHUTTER,
    gain: int = DEFAULT_GAIN,
    aec: str = DEFAULT_AEC,
    ble_mtu: int = DEFAULT_BLE_MTU,
    chunk_delay_ms: int = DEFAULT_CHUNK_DELAY_MS,
    quality: int | None = None,
) -> str:
    """Take a photo with the flash LED using the ESP32-S3 BLE camera.

    Use this tool ONLY when the user explicitly asks to use the flash,
    for example "take a photo with the flash" or "turn the flash on and
    take a photo". Do not use it for ordinary photos or infer flash from
    low light. For a normal photo, call ``take_picture`` instead.

    The firmware handles flash enable, exposure settling, frame capture,
    and flash disable atomically, so the host sends one BLE command.

    This tool does NOT describe the photo. To describe it, call
    ``describe_picture`` with the returned file path.

    Args:
        output_path: File path to save the JPEG image. Default: snapshot.jpg
        shutter: Exposure value used when the flash is off (lower = shorter
            exposure = less motion blur, but darker). Default: 300.
        gain: Sensor gain/ISO used when the flash is off (higher = brighter
            but noisier). Default: 20.
        aec: Auto-exposure mode used when the flash is off ("on"/"off").
            Default: "off".
        ble_mtu: Local ATT MTU cap (23-517). Default: 517.
        chunk_delay_ms: Delay between BLE notify chunks. Lower = faster
            transfer. Default: 2.
        quality: JPEG quality (0-63). When None (default), the firmware's
            persisted value is left unchanged.

    Returns:
        str: Confirmation message with file path and size in bytes.
             Example: "Photo saved to snapshot.jpg (4521 bytes, flash on)"
    """
    device = os.getenv("CAMERA_AGENT_DEVICE", "ApiBLE")
    settings = _build_settings(
        shutter=shutter,
        gain=gain,
        aec=aec,
        ble_mtu=ble_mtu,
        chunk_delay_ms=chunk_delay_ms,
        quality=quality,
    )

    async with _camera_session(device) as client:
        await client.set_camera_settings(**settings)
        info = await client.capture_with_flash(output_path)

    size = info.get("size", "unknown")
    return f"Photo saved to {output_path} ({size} bytes, flash on)"
