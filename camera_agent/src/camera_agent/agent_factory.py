"""Assembles a ManagedAgent with the camera tool."""

import os

from agent_harness import ManagedAgent
from agent_harness.model_config import ModelConfig
from agent_harness.prompts import StaticPrompts
from agent_harness.tools import ToolRegistry

from camera_agent.tools import describe_picture, take_picture, take_picture_with_flash


def create_agent() -> ManagedAgent:
    """Build and return a ManagedAgent configured for camera control.

    Reads configuration from environment variables:
        CAMERA_AGENT_PROVIDER     — LLM provider (default: openai)
        CAMERA_AGENT_MODEL        — main model name
                                    (default: Qwen3-VL-4B-Instruct-4bit)
        CAMERA_AGENT_BASE_URL     — OpenAI-compatible endpoint, e.g.
                                    http://localhost:8000/v1 for vLLM
        CAMERA_AGENT_API_KEY      — API key for the endpoint (optional for vLLM)
        CAMERA_AGENT_VISION_MODEL — vision model used by describe_picture
                                    (default: Qwen3-VL-4B-Instruct-4bit, read
                                    inside the tool)

    The main model handles conversation and tool calling. describe_picture
    delegates to the vision model, which receives the actual JPEG bytes, so
    any description of a photo must come from that tool rather than from the
    main model. A multimodal model such as Qwen3-VL can serve both roles.
    """
    provider = os.getenv("CAMERA_AGENT_PROVIDER", "openai")
    model = os.getenv("CAMERA_AGENT_MODEL", "Qwen3-VL-4B-Instruct-4bit")
    base_url = os.getenv("CAMERA_AGENT_BASE_URL") or None
    api_key = os.getenv("CAMERA_AGENT_API_KEY") or None
    temperature = float(os.getenv("CAMERA_AGENT_TEMPERATURE", "0"))

    return (
        ManagedAgent()
        .with_model(
            ModelConfig(
                provider=provider,
                model_name=model,
                base_url=base_url,
                api_key=api_key,
            )
        )
        .with_model_settings({"temperature": temperature})
        .with_tools(ToolRegistry().add_many(
            take_picture, take_picture_with_flash, describe_picture))
        .with_prompts(StaticPrompts(
            "You are a camera assistant for an ESP32-S3 BLE camera.\n"
            "Call tools immediately without narrating your plan.\n"
            "To capture a normal photo, call take_picture exactly once.\n"
            "Call take_picture_with_flash ONLY when the user explicitly asks "
            "to use or turn on the flash. Never infer flash from low light.\n"
            "If the user asks to turn the flash on and take a photo, call "
            "take_picture_with_flash exactly once.\n"
            "Never call both capture tools for the same request.\n"
            "If the user asks for a photo, call the matching capture tool; "
            "never claim you cannot take one.\n"
            "Never claim you cannot control the flash; use the appropriate tool.\n"
            "To answer anything about an existing file, call describe_picture on "
            "that file only. Never call a capture tool in this case.\n"
            "Only when asked to both capture and describe, call the appropriate "
            "capture tool then describe_picture.\n"
            "You cannot see images. Use describe_picture to learn what is in a "
            "photo, then answer the user's question using only facts from that "
            "description. Do not add details not present in the description."
        ))
    )
