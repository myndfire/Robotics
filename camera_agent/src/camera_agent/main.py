#!/usr/bin/env python3
"""CLI for the camera agent — natural language camera control via BLE.

Usage:
    uv run python -m camera_agent.main

Prerequisites:
    - ESP32-S3 running CameraBLE firmware (flashed from Robotics/firmware/apps/CameraBLE)
    - Bluetooth enabled on this computer
    - LLM configured via CAMERA_AGENT_PROVIDER / CAMERA_AGENT_MODEL env vars (or .env)
"""

import asyncio
from dotenv import load_dotenv

load_dotenv()  # read .env into os.environ before any module imports env vars

from camera_agent.agent_factory import create_agent
from agent_harness.memory import MessageHistory


def _print_response(text: str) -> None:
    """Print agent response in bright cyan to distinguish it from logs."""
    print(f"\033[96m{text}\033[0m")


async def main():
    agent = create_agent()

    print("Camera Agent")
    print("  Type 'exit' or 'quit' to stop.")
    print("  Ensure CameraBLE firmware is running on ESP32-S3.")
    print()

    while True:
        try:
            prompt = input("> ")
        except (EOFError, KeyboardInterrupt):
            print()
            break

        if prompt.lower() in ("exit", "quit", ""):
            break

        try:
            result = await agent.run(prompt, MessageHistory(), "camera-session")
            _print_response(result.output)
        except Exception as e:
            print(f"Error: {e}")


if __name__ == "__main__":
    asyncio.run(main())
