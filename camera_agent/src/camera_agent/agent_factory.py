"""Assembles a ManagedAgent with the camera tool."""

import os

from agent_harness import ManagedAgent
from agent_harness.model_config import ModelConfig
from agent_harness.prompts import StaticPrompts
from agent_harness.tools import ToolRegistry

from camera_agent.tools import describe_picture, take_picture


def create_agent() -> ManagedAgent:
    """Build and return a ManagedAgent configured for camera control.

    Reads configuration from environment variables:
        CAMERA_AGENT_PROVIDER     — LLM provider (default: ollama)
        CAMERA_AGENT_MODEL        — main model name (default: gpt-oss:20b)
        CAMERA_AGENT_VISION_MODEL — vision model used by describe_picture
                                    (default: llava, read inside the tool)

    Two models are in play. The main model handles conversation and tool
    calling but cannot see image content. describe_picture delegates to a
    vision model that receives the actual JPEG bytes, so any description of
    a photo must come from that tool rather than from the main model.
    """
    provider = os.getenv("CAMERA_AGENT_PROVIDER", "ollama")
    model = os.getenv("CAMERA_AGENT_MODEL", "gpt-oss:20b")
    temperature = float(os.getenv("CAMERA_AGENT_TEMPERATURE", "0"))

    return (
        ManagedAgent()
        .with_model(ModelConfig(provider=provider, model_name=model))
        .with_model_settings({"temperature": temperature})
        .with_tools(ToolRegistry().add_many(take_picture, describe_picture))
        .with_prompts(StaticPrompts(
            "You are a camera assistant for an ESP32-S3 BLE camera.\n"
            "Call tools immediately without narrating your plan.\n"
            "To capture a new photo, call take_picture exactly once.\n"
            "Never call it more than once for the same request.\n"
            "To answer anything about an existing file, call describe_picture on "
            "that file only. Never call take_picture in this case.\n"
            "Only when asked to both capture and describe, call take_picture then "
            "describe_picture.\n"
            "You cannot see images. Use describe_picture to learn what is in a "
            "photo, then answer the user's question using only facts from that "
            "description. Do not add details not present in the description."
        ))
    )
