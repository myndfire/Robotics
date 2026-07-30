"""Shared debugging infrastructure for camera agent tools.

Provides the ``log_tool_call`` decorator used by both ``capture.py`` and
``describe.py``. The decorator is a no-op when ``CAMERA_AGENT_DEBUG`` is
off, so production runs incur zero overhead.
"""

import functools
import inspect
import os
import time

import structlog

logger = structlog.get_logger("camera_agent.tools")


def is_debug_enabled() -> bool:
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
        if not is_debug_enabled():
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
