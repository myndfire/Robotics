"""Vision model access for image description.

Counterpart to :mod:`camera_agent.agent_factory`. That module configures the
conversational model through ``ManagedAgent``; this module configures the vision
model the same way. Both read their model choice from the environment so
neither hardcodes a model name.

The prompt passed to ``agent.run()`` is a ``Sequence[UserContent]`` — a
``TextPart`` with the question, plus an ``ImageUrl`` pointing at the base64
encoded JPEG. ``ManagedAgent.run()`` accepts this because the upstream
``agent_harness`` has been widened to support multimodal prompts.
"""

from __future__ import annotations

import base64
import os
from dataclasses import dataclass

from pydantic_ai.messages import ImageUrl

from agent_harness import ManagedAgent
from agent_harness.memory import MessageHistory
from agent_harness.model_config import ModelConfig
from agent_harness.prompts import StaticPrompts

_DEFAULT_MODEL = "llava"

_DEFAULT_PROMPT = (
    "Describe what you see in this image. Be specific and factual. "
    "If the image is blurry, dark, or unclear, say so plainly instead of guessing."
)


def resolve_model() -> str:
    """Resolve which vision model to use.

    Returns:
        str: Model name from CAMERA_AGENT_VISION_MODEL, defaulting to llava.
    """
    return os.getenv("CAMERA_AGENT_VISION_MODEL", _DEFAULT_MODEL)


@dataclass
class VisionResult:
    """Outcome of a vision model request.

    Attributes:
        description: What the model reported seeing. Empty when ``ok`` is False.
        error:       Operator-actionable explanation of the failure. Empty when
                     ``ok`` is True.
    """

    description: str = ""
    error: str = ""

    @property
    def ok(self) -> bool:
        """Whether a description was produced.

        Returns:
            bool: True when a non-empty description is available.
        """
        return not self.error


def create_vision_agent() -> ManagedAgent:
    """Build a ManagedAgent for the vision model.

    Uses the same pattern as :func:`camera_agent.agent_factory.create_agent`,
    but configured for image description rather than conversation.

    Returns:
        ManagedAgent: Ready to run with a multimodal prompt.
    """
    return (
        ManagedAgent()
        .with_model(ModelConfig(provider="ollama", model_name=resolve_model()))
        .with_prompts(
            StaticPrompts(
                "Describe the image factually. Say if unclear."
            )
        )
    )


async def describe_image(image_bytes: bytes, question: str = "") -> VisionResult:
    """Ask the vision model what is in an image.

    Args:
        image_bytes: Raw JPEG (or other model-supported format) content.
        question:    Specific question about the image. When empty, a general
                     factual description is requested.

    Returns:
        VisionResult: Description on success, or an operator-actionable error.
    """
    if not image_bytes:
        return VisionResult(error="No image data to describe (0 bytes).")

    agent = create_vision_agent()
    b64 = base64.b64encode(image_bytes).decode("ascii")
    prompt = [
        question.strip() or _DEFAULT_PROMPT,
        ImageUrl(url=f"data:image/jpeg;base64,{b64}"),
    ]

    try:
        result = await agent.run(
            prompt,
            MessageHistory(),
            "vision-session",
        )
    except Exception as exc:
        model = resolve_model()
        detail = str(exc)
        if "404" in detail and "model" in detail.lower():
            return VisionResult(
                error=(
                    f"Vision model '{model}' returned 404. "
                    f"Confirm the model is installed with: ollama pull {model}"
                )
            )
        if "ConnectError" in type(exc).__name__:
            return VisionResult(
                error=(
                    f"Could not reach Ollama ({type(exc).__name__}). "
                    "Confirm Ollama is running, or set OLLAMA_BASE_URL."
                )
            )
        return VisionResult(
            error=f"Vision model '{model}' failed: {detail}"
        )

    if not result.success:
        return VisionResult(
            error=f"Vision model '{resolve_model()}' returned an unsuccessful result."
        )

    description = (result.output or "").strip()
    if not description:
        return VisionResult(
            error=f"Vision model '{resolve_model()}' returned an empty description."
        )

    return VisionResult(description=description)
