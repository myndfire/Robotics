"""Vision image description tool — ``describe_picture``.

Delegates to :mod:`camera_agent.vision` which runs a separate
``ManagedAgent`` with the vision model (e.g. ``llava``). The main
conversational model never sees image pixels, so description must
come from this tool rather than from its imagination.
"""

from pathlib import Path

from camera_agent.tools._debug import log_tool_call
from camera_agent.vision import describe_image


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
