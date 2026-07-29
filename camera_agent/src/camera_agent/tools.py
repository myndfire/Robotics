"""Tools exposed to the managed agent — BLE camera capture and image description.

Two-model design:

    take_picture      -> runs on the main agent's model (text, e.g. gpt-oss:20b).
                         Only moves bytes off the ESP32 and onto disk.
    describe_picture  -> delegates to the vision model via camera_agent.vision.
                         The main model never sees pixels, so description must
                         come from here rather than from its imagination.

Each tool owns only its I/O concerns. Camera transport lives in
CameraBleClient; vision transport lives in camera_agent.vision.
"""

import functools
import inspect
import os
import time
from pathlib import Path

import structlog

from embedded_system_services.camera_ble import CameraBleClient

from camera_agent.vision import describe_image

logger = structlog.get_logger("camera_agent.tools")


def _is_debug() -> bool:
    """Check whether CAMERA_AGENT_DEBUG is enabled.

    Returns:
        bool: True when the environment variable is set to "1", "true", or "yes".
    """
    val = os.getenv("CAMERA_AGENT_DEBUG", "").lower()
    return val in ("1", "true", "yes")


def _args_preview(bound: inspect.BoundArguments) -> dict:
    """Produce a serialisable preview of bound arguments.

    Filters out large binary values and truncates strings.
    """
    preview: dict = {}
    for k, v in bound.arguments.items():
        s = str(v)
        if len(s) > 200:
            s = s[:197] + "..."
        preview[k] = s
    return preview


def log_tool_call(func):
    """Decorator that logs tool entry, exit, duration and result.

    Only emits when CAMERA_AGENT_DEBUG is enabled.
    """

    @functools.wraps(func)
    async def wrapper(*args, **kwargs):
        if not _is_debug():
            return await func(*args, **kwargs)

        sig = inspect.signature(func)
        bound = sig.bind(*args, **kwargs)
        bound.apply_defaults()
        tool_name = func.__name__

        logger.info(
            "tool_call_start",
            tool=tool_name,
            arguments=_args_preview(bound),
        )
        start = time.perf_counter()

        try:
            result = await func(*args, **kwargs)
            duration = time.perf_counter() - start
            logger.info(
                "tool_call_end",
                tool=tool_name,
                success=True,
                duration_seconds=round(duration, 3),
                result_preview=str(result)[:500],
            )
            return result
        except Exception as exc:
            duration = time.perf_counter() - start
            logger.info(
                "tool_call_end",
                tool=tool_name,
                success=False,
                duration_seconds=round(duration, 3),
                error=str(exc),
            )
            raise

    return wrapper


@log_tool_call
async def take_picture(output_path: str = "snapshot.jpg") -> str:
    """Take a photo using the ESP32-S3 BLE camera.

    Connects to the ESP32-S3 running CameraBluetoothServer firmware
    (advertises as ESP32-CAM), captures a single JPEG snapshot, and
    saves it to disk.

    This tool does NOT describe the photo. To describe it, call
    describe_picture with the returned file path.

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


@log_tool_call
async def describe_picture(image_path: str = "snapshot.jpg", question: str = "") -> str:
    """Describe the contents of a photo on disk using a vision model.

    Sends the actual JPEG bytes to a local vision model (llava by default)
    and returns what that model reports seeing. Always use this tool to
    describe an image — never describe an image from memory or assumption,
    because the calling model cannot see image content.

    Args:
        image_path: Path to the JPEG to describe. Default: snapshot.jpg
        question: Optional specific question about the image, e.g.
                  "How many people are visible?". When empty, a general
                  description is requested.

    Returns:
        str: The vision model's description, or an error message explaining
             why the description could not be produced.
    """
    path = Path(image_path)
    if not path.is_file():
        return (
            f"Cannot describe '{image_path}' — file does not exist. "
            "Call take_picture first to capture an image."
        )

    result = await describe_image(path.read_bytes(), question)
    if not result.ok:
        return f"Cannot describe '{image_path}' — {result.error}"

    return result.description
