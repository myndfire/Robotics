# Camera Agent

A managed AI agent that controls an ESP32-S3 BLE camera using natural language. It uses a conversational model for tool calling and a vision model for image understanding; a single multimodal model (the default, `Qwen3-VL-4B-Instruct-4bit`) can serve both roles.

## Architecture

### Model Design

The default setup uses one multimodal model served by vLLM. The two roles are still separate agents, so the vision model can be pointed at a different endpoint or model via environment variables.

```
User: "take a picture and describe it"
   │
   ▼
┌─────────────────────────────────────────────────┐
│  Main Agent (CAMERA_AGENT_MODEL)                │
│  Default: Qwen3-VL-4B-Instruct-4bit via vLLM    │
│  • Conversational reasoning                       │
│  • Tool calling (take_picture, describe_picture) │
│  • Answers user questions                        │
└──────────────────┬──────────────────────────────┘
                   │
       ┌───────────┴───────────┐
       │                       │
       ▼                       ▼
┌──────────────┐      ┌──────────────┐
│ take_picture │      │describe_picture│
│  (BLE tool)  │      │  (Vision tool) │
└──────┬───────┘      └──────┬───────┘
       │                      │
       ▼                      ▼
  ApiBLE               Qwen3-VL
  (BLE device)          (Vision model)
       │                      │
       ▼                      ▼
  snapshot.jpg          Description
  (4-6KB JPEG)          (Text from Qwen3-VL)
```

### Request Flow

1. **User speaks** → Main agent (text model) interprets intent
2. **Tool selection** → Agent decides which tool(s) to call
3. **Tool execution**:
   - `take_picture`: Connects via BLE, captures JPEG, saves to disk
   - `describe_picture`: Reads JPEG, sends to vision model, returns description
4. **Response generation** → Main agent answers using tool results

### Code Structure

```
camera_agent/
├── src/camera_agent/
│   ├── main.py              # CLI entry point, loads .env
│   ├── agent_factory.py     # Creates ManagedAgent with tools + system prompt
│   ├── tools/               # Tools exposed to the agent
│   │   ├── __init__.py      # Re-exports take_picture, describe_picture
│   │   ├── _debug.py        # Shared logging decorator (CAMERA_AGENT_DEBUG)
│   │   ├── capture.py       # take_picture: BLE camera capture
│   │   └── describe.py      # describe_picture: Vision model delegation
│   └── vision.py            # Vision agent: ManagedAgent + ImageUrl
├── .env                     # Environment variables
├── pyproject.toml           # Dependencies (agent-harness, embedded-system-services)
└── README.md
```

## Prerequisites

| Component | Requirement |
|---|---|
| **Hardware** | ESP32-S3 CAM board with OV2640/OV3660 camera |
| **Firmware** | ApiBLE firmware flashed on ESP32-S3 |
| **Host OS** | macOS, Linux, or Windows with Bluetooth |
| **Python** | 3.11+ with [uv](https://docs.astral.sh/uv/) |
| **LLM** | vLLM (or any OpenAI-compatible server) serving a multimodal model such as `Qwen3-VL-4B-Instruct-4bit` |

## Setup

### 1. Flash the ApiBLE Firmware

```bash
cd /path/to/Robotics/firmware/apps/ApiBLE
uv run pio run --target upload
```

The ESP32 will advertise as `ApiBLE` over BLE after boot. Verify by checking the serial monitor:
```bash
uv run pio run --target monitor
# You should see: "Advertising as ApiBLE"
```

### 2. Serve a Model with vLLM

The agent talks to any OpenAI-compatible endpoint. The default configuration targets vLLM on port 8000 with tool calling enabled:

```bash
vllm serve /path/to/Qwen3-VL-4B-Instruct-4bit \
  --served-model-name Qwen3-VL-4B-Instruct-4bit \
  --enable-auto-tool-choice \
  --tool-call-parser hermes
```

Verify the server is up and note the served model name:
```bash
curl http://localhost:8000/v1/models
```

Tool calling is required: without `--enable-auto-tool-choice` and a matching `--tool-call-parser`, the model cannot call `take_picture` or `describe_picture`. Ollama remains usable by pointing `CAMERA_AGENT_PROVIDER` / `CAMERA_AGENT_BASE_URL` at it.

### 3. Configure Environment

```bash
cd /path/to/Robotics/camera_agent
cp .env.example .env
```

Edit `.env`:
```bash
# LLM configuration (vLLM exposes an OpenAI-compatible API)
CAMERA_AGENT_PROVIDER=openai
CAMERA_AGENT_MODEL=Qwen3-VL-4B-Instruct-4bit
CAMERA_AGENT_BASE_URL=http://localhost:8000/v1
CAMERA_AGENT_API_KEY=EMPTY

# Vision model (multimodal; can be a different endpoint/model if needed)
CAMERA_AGENT_VISION_MODEL=Qwen3-VL-4B-Instruct-4bit

# BLE device name (must match firmware)
CAMERA_AGENT_DEVICE=ApiBLE

# Debug mode (set to 1 to see tool execution)
CAMERA_AGENT_DEBUG=0
```

### 4. Install Dependencies

```bash
uv sync
```

This installs:
- `agent-harness` — managed agent framework (from GitHub)
- `embedded-system-services` — BLE client library (from local workspace)

### 5. Run the Agent

```bash
uv run python -m camera_agent.main
```

## Usage

### Basic Commands

```
> take a photo
Photo saved to snapshot.jpg (5619 bytes)

> take a picture and describe it
The image shows an interior space with a patterned carpet...

> take a picture of something moving fast
Photo saved to snapshot.jpg (6120 bytes)

> what color are the walls?
The walls appear to be white or off-white.

> exit
```

### Prompt Patterns

| User Intent | Example Prompt | Tools Called |
|---|---|---|
| Capture only | "take a photo" | `take_picture` |
| Fast-moving object | "take a picture of something moving fast" | `take_picture` (short exposure applied by default) |
| Describe only | "describe snapshot.jpg" | `describe_picture` |
| Capture + Describe | "take a picture and describe it" | `take_picture` → `describe_picture` |
| Question about image | "is there a carpet?" | `describe_picture` |
| Conditional | "if you see a cat, say meow" | `take_picture` → `describe_picture` → reasoning |

## Configuration

| Variable | Default | Description |
|---|---|---|
| `CAMERA_AGENT_PROVIDER` | `openai` | Provider name; `openai` for vLLM's OpenAI-compatible API |
| `CAMERA_AGENT_MODEL` | `Qwen3-VL-4B-Instruct-4bit` | Main model for conversation and tool calling |
| `CAMERA_AGENT_BASE_URL` | `http://localhost:8000/v1` | OpenAI-compatible endpoint (vLLM) |
| `CAMERA_AGENT_API_KEY` | `EMPTY` | API key; vLLM accepts any non-empty value |
| `CAMERA_AGENT_VISION_MODEL` | `Qwen3-VL-4B-Instruct-4bit` | Vision model for image description |
| `CAMERA_AGENT_VISION_PROVIDER` | falls back to `CAMERA_AGENT_PROVIDER` | Optional provider override for the vision agent |
| `CAMERA_AGENT_VISION_BASE_URL` | falls back to `CAMERA_AGENT_BASE_URL` | Optional endpoint override for the vision agent |
| `CAMERA_AGENT_VISION_API_KEY` | falls back to `CAMERA_AGENT_API_KEY` | Optional API key override for the vision agent |
| `CAMERA_AGENT_DEVICE` | `ApiBLE` | BLE device name to scan for |
| `CAMERA_AGENT_AEC` | `off` | Auto-exposure applied on capture (`on`/`off`) |
| `CAMERA_AGENT_SHUTTER` | `300` | Exposure value applied on capture (lower = less motion blur, darker) |
| `CAMERA_AGENT_GAIN` | `20` | Sensor gain applied on capture (higher = brighter, noisier) |
| `CAMERA_AGENT_BLE_MTU` | `517` | Local ATT MTU cap (23–517) for faster BLE transfer |
| `CAMERA_AGENT_CHUNK_DELAY_MS` | `2` | BLE notify chunk delay in ms (lower = faster transfer) |
| `CAMERA_AGENT_DEBUG` | `0` | Enable tool execution logging (1/true/yes) |

## Debug Mode

Set `CAMERA_AGENT_DEBUG=1` in `.env` to see tool execution:

```
> take a photo
🔧  [take_picture] start  args={'output_path': 'snapshot.jpg'}
Scanning for 'ApiBLE'...
Found: ApiBLE
Connected: True (ATT MTU: 247)
Camera settings sent: aec=off,shutter=300,gain=20,ble_mtu=517,chunk_delay_ms=2
Frame saved: snapshot.jpg (5619 bytes)
🔧  [take_picture] end  success=True  duration=2.341s
Photo saved to snapshot.jpg (5619 bytes)
```

**Useful for:**
- Confirming the LLM actually calls tools (not just talks about them)
- Seeing what arguments the LLM passes
- Measuring BLE capture vs vision inference latency
- Diagnosing why a tool was skipped or failed

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `Device 'ApiBLE' not found` | Board not powered / not advertising | Check USB, press RST, verify firmware flashed |
| `Vision model '...' returned 404` | Served model name mismatch | Check `curl http://localhost:8000/v1/models` and match `CAMERA_AGENT_VISION_MODEL` to the served name |
| `Could not reach the model server` | vLLM not running / wrong `CAMERA_AGENT_BASE_URL` | Start vLLM and verify with `curl http://localhost:8000/v1/models` |
| Agent describes things not in photo | Main model hallucinating | Check `describe_picture` output in debug mode; vision model sees pixels, main model only sees text |
| Agent takes 2+ photos for 1 request | Main model misunderstanding | System prompt says "exactly once"; shorten prompt if too complex |
| Agent outputs raw JSON instead of calling tool | Tool calling disabled on the server, or prompt too long | Restart vLLM with `--enable-auto-tool-choice --tool-call-parser hermes`; keep the prompt concise |
| Description seems wrong | Vision model misidentifying | Expected for small objects in 320x240 frames; increase camera quality in firmware |
| Fast object still blurry | Shutter too long | Lower `CAMERA_AGENT_SHUTTER` (e.g. 150) and raise `CAMERA_AGENT_GAIN` to compensate; tune per scene via `take_picture(shutter=..., gain=...)` |
| Fast-motion shot too dark / noisy | Gain/ISO too low or high | Raise `shutter` or `gain`; for noise, raise `shutter` and lower `gain` |
| Board connects but no image received | Missing BLE2902 descriptor | Verify CCCD on CA03 characteristic in firmware |

## Architecture Details

### Why Two Agents?

| Model | Role | Input | Output |
|---|---|---|---|
| **Qwen3-VL-4B-Instruct-4bit** | Conversation, tool calling | Text prompt | Tool calls + text answer |
| **Qwen3-VL-4B-Instruct-4bit** | Image understanding | JPEG pixels | Text description |

The two roles run as separate `ManagedAgent` instances so the vision model can be swapped independently (via `CAMERA_AGENT_VISION_*`) without touching the conversational agent. A multimodal model like Qwen3-VL serves both roles in the default setup; if you use a text-only main model, point the vision agent at a separate vision model instead.

### Data Flow

```
User Text
    │
    ▼
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│ Main Agent  │────→│ take_picture│────→│ ApiBLE     │
│ (Qwen3-VL)  │     │ (BLE tool)  │     │ (Hardware)  │
└─────────────┘     └─────────────┘     └──────┬──────┘
    │                                            │
    │         ┌──────────────────────────────────┘
    │         │
    │    snapshot.jpg (JPEG)
    │         │
    │    ┌────┴────┐
    └───→│ describe│────→ Qwen3-VL (vision model)
         │ _picture│         └──→ Text description
         └────┬────┘
              │
    ┌─────────┘
    ▼
User Answer (text, based on description)
```

### Tool Design

**`take_picture` (capture.py):**
- No vision capability — only moves bytes from ESP32 to disk
- Applies fast-motion exposure settings by default (`aec=off`, `shutter=300`, `gain=20`) to freeze moving objects, plus BLE throughput knobs (`ble_mtu=517`, `chunk_delay_ms=2`) for a faster transfer. All are persisted in firmware NVS and overridable per-call (the model can pass `shutter`/`gain`) or via `CAMERA_AGENT_*` env vars.
- Returns: `"Photo saved to {path} ({size} bytes)"`
- Overwrites existing file (by design)

**`describe_picture` (describe.py):**
- No BLE capability — only reads file and calls vision model
- Returns: Description text or error message
- Vision model is a separate `ManagedAgent` using `ImageUrl` prompt

**Separation of concerns:** Each tool does one thing. The LLM composes them.

## Development

### Adding a New Tool

1. Create handler in `src/camera_agent/tools/`:
```python
# tools/my_tool.py
from camera_agent.tools._debug import log_tool_call

@log_tool_call
async def my_tool(param: str) -> str:
    return f"Result: {param}"
```

2. Export in `src/camera_agent/tools/__init__.py`:
```python
from camera_agent.tools.my_tool import my_tool
__all__ = ["take_picture", "describe_picture", "my_tool", "log_tool_call"]
```

3. Register in `src/camera_agent/agent_factory.py`:
```python
from camera_agent.tools import my_tool
# ...
.with_tools(ToolRegistry().add_many(take_picture, describe_picture, my_tool))
```

### Testing Without Hardware

You can test the vision pipeline without the ESP32:
```python
from camera_agent.vision import describe_image

with open("existing_photo.jpg", "rb") as f:
    result = await describe_image(f.read(), "What do you see?")
    print(result.description)
```

## Related Projects

| Project | Relationship |
|---|---|
| [pydanticai-fluent](https://github.com/myndfire/pydanticai-fluent) | Agent framework (`agent-harness`) |
| [Robotics/firmware](https://github.com/myndfire/Robotics) | ESP32-S3 firmware (ApiBLE, device drivers) |
| [Robotics/python-libs](https://github.com/myndfire/Robotics/tree/main/python-libs) | BLE client library (`embedded-system-services`) |

## License

Apache 2.0 — See [LICENSE](../LICENSE)
