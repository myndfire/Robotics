"""Tools exposed to the managed agent.

Each submodule owns a single tool concern:

    capture   → BLE camera snapshot (``take_picture``)
    describe  → Vision model description (``describe_picture``)
    _debug    → Shared logging decorator (``log_tool_call``)

Import from the package level for convenience:

    from camera_agent.tools import take_picture, describe_picture
"""

from camera_agent.tools.capture import take_picture
from camera_agent.tools.describe import describe_picture
from camera_agent.tools._debug import log_tool_call

__all__ = ["take_picture", "describe_picture", "log_tool_call"]
