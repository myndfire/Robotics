# Camera Agent

A managed AI agent that controls an ESP32-S3 BLE camera using natural language. When you ask it to take a picture, the LLM calls the `take_picture` tool, which connects to the ESP32-CAM over BLE and captures a JPEG snapshot.

## Prerequisites

- **ESP32-S3 CAM** board running the **CameraBLE** firmware
- **Bluetooth** enabled on your laptop
- **Python 3.11+** with [uv](https://docs.astral.sh/uv/)
- An **LLM** (Ollama local, or any [supported provider](https://github.com/myndfire/pydanticai-fluent/blob/master/USAGE.md#supported-providers))

## Setup

### 1. Flash the CameraBLE firmware

```bash
git clone https://github.com/myndfire/Robotics.git
cd Robotics/firmware/apps/CameraBLE
uv run pio run --target upload
```

The ESP32 will advertise as `ESP32-CAM` over BLE after boot.

### 2. Configure the LLM

```bash
cp .env.example .env
```

Edit `.env` — set your LLM provider and model. Defaults to `ollama:gpt-oss:20b`. For OpenAI:

```bash
CAMERA_AGENT_PROVIDER=openai
CAMERA_AGENT_MODEL=gpt-4o
export OPENAI_API_KEY=sk-...
```

### 3. Install dependencies

```bash
uv sync
```

This clones and installs both dependencies from GitHub:
- `agent-harness` — the managed agent framework
- `embedded-system-services` — the BLE camera client library

### 4. Run the agent

```bash
uv run python -m camera_agent.main
```

## Usage

```
> Take a picture
Photo saved to snapshot.jpg (4521 bytes)

> Take a photo and save it as kitchen.jpg
Photo saved to kitchen.jpg (4490 bytes)

> exit
```

The agent understands natural language — you can phrase the request however you want. Any prompt that asks for a photo will trigger the `take_picture` tool.

## Configuration

| Env Variable | Default | Description |
|---|---|---|
| `CAMERA_AGENT_PROVIDER` | `ollama` | LLM provider. See supported providers in the pydanticai-fluent USAGE.md. |
| `CAMERA_AGENT_MODEL` | `gpt-oss:20b` | Model name for the selected provider. |
| `CAMERA_AGENT_DEVICE` | `ESP32-CAM` | BLE device name to scan for. Change if your board advertises differently. |
