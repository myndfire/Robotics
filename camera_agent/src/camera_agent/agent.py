"""Assembles a ManagedAgent with the camera tool."""

import os
from agent_harness import ManagedAgent
from agent_harness.tools import ToolRegistry
from agent_harness.prompts import StaticPrompts
from agent_harness.model_config import ModelConfig
from camera_agent.tools import take_picture


def create_agent():
    """Build and return a ManagedAgent configured for camera control.

    Reads configuration from environment variables:
        CAMERA_AGENT_PROVIDER — LLM provider (default: ollama)
        CAMERA_AGENT_MODEL    — model name (default: gpt-oss:20b)

    The agent has a single tool (take_picture) that captures photos
    from the ESP32-S3 BLE camera when the user requests it.
    """
    provider = os.getenv("CAMERA_AGENT_PROVIDER", "ollama")
    model = os.getenv("CAMERA_AGENT_MODEL", "gpt-oss:20b")

    return (
        ManagedAgent()
        .with_model(ModelConfig(provider=provider, model_name=model))
        .with_tools(ToolRegistry().add(take_picture))
        .with_prompts(StaticPrompts(
            "You are a camera assistant. You control an ESP32-S3 BLE camera. "
            "When the user asks to take a photo, picture, snapshot, or capture "
            "an image, use the take_picture tool immediately — do not describe "
            "what you will do, just call the tool."
        ))
    )
