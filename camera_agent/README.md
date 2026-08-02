# Camera Agent

A managed AI agent that controls an ESP32-S3 BLE camera using natural language. It uses two models: a text model for conversation and a vision model for image understanding.

## Architecture

### Two-Model Design

The agent uses two specialized models because the model that is good at conversation is not the model that can see images:

```
User: "take a picture and describe it"
   │
   ▼
┌─────────────────────────────────────────────────┐
│  Main Agent (CAMERA_AGENT_MODEL)                │
│  Default: gpt-oss:20b via Ollama                │
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
  ApiBLE               llava
  (BLE device)          (Vision model)
       │                      │
       ▼                      ▼
  snapshot.jpg          Description
  (4-6KB JPEG)          (Text from llava)
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
| **LLM** | Ollama running locally (for both text and vision models) |

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

### 2. Install Ollama and Pull Models

```bash
# Install Ollama (macOS)
brew install ollama

# Start Ollama
ollama serve

# Pull models (in another terminal)
ollama pull gpt-oss:20b    # Main conversation model
ollama pull llava          # Vision model for image description
```

### 3. Configure Environment

```bash
cd /path/to/Robotics/camera_agent
cp .env.example .env
```

Edit `.env`:
```bash
# LLM configuration
CAMERA_AGENT_PROVIDER=ollama
CAMERA_AGENT_MODEL=gpt-oss:20b

# Vision model (always via Ollama)
CAMERA_AGENT_VISION_MODEL=llava

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

> what color are the walls?
The walls appear to be white or off-white.

> exit
```

### Prompt Patterns

| User Intent | Example Prompt | Tools Called |
|---|---|---|
| Capture only | "take a photo" | `take_picture` |
| Describe only | "describe snapshot.jpg" | `describe_picture` |
| Capture + Describe | "take a picture and describe it" | `take_picture` → `describe_picture` |
| Question about image | "is there a carpet?" | `describe_picture` |
| Conditional | "if you see a cat, say meow" | `take_picture` → `describe_picture` → reasoning |

## Configuration

| Variable | Default | Description |
|---|---|---|
| `CAMERA_AGENT_PROVIDER` | `ollama` | LLM provider (ollama, openai, anthropic, etc.) |
| `CAMERA_AGENT_MODEL` | `gpt-oss:20b` | Main model for conversation and tool calling |
| `CAMERA_AGENT_VISION_MODEL` | `llava` | Vision model for image description (always Ollama) |
| `CAMERA_AGENT_DEVICE` | `ApiBLE` | BLE device name to scan for |
| `OLLAMA_BASE_URL` | `http://localhost:11434` | Ollama API endpoint |
| `CAMERA_AGENT_DEBUG` | `0` | Enable tool execution logging (1/true/yes) |

## Debug Mode

Set `CAMERA_AGENT_DEBUG=1` in `.env` to see tool execution:

```
> take a photo
🔧  [take_picture] start  args={'output_path': 'snapshot.jpg'}
Scanning for 'ApiBLE'...
Found: ApiBLE
Connected: True
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
| `Vision model 'llava' returned 404` | llava not pulled | `ollama pull llava` |
| `Could not reach Ollama` | Ollama not running | Start `ollama serve` |
| Agent describes things not in photo | Main model hallucinating | Check `describe_picture` output in debug mode; vision model sees pixels, main model only sees text |
| Agent takes 2+ photos for 1 request | Main model misunderstanding | System prompt says "exactly once"; shorten prompt if too complex |
| Agent outputs raw JSON instead of calling tool | System prompt too long for model | Keep prompt concise (under 100 words for gpt-oss:20b) |
| Description seems wrong | Vision model misidentifying | Expected for small objects in 320x240 frames; increase camera quality in firmware |
| Board connects but no image received | Missing BLE2902 descriptor | Verify CCCD on CA03 characteristic in firmware |

## Architecture Details

### Why Two Models?

| Model | Role | Input | Output | Size |
|---|---|---|---|---|
| **gpt-oss:20b** | Conversation, tool calling | Text prompt | Tool calls + text answer | ~13GB |
| **llava** | Image understanding | JPEG pixels | Text description | ~4.7GB |

A single model cannot do both well. gpt-oss:20b is text-only (would hallucinate descriptions). llava is vision-only (weak at tool calling and conversation).

### Data Flow

```
User Text
    │
    ▼
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│ Main Agent  │────→│ take_picture│────→│ ApiBLE     │
│ (gpt-oss)   │     │ (BLE tool)  │     │ (Hardware)  │
└─────────────┘     └─────────────┘     └──────┬──────┘
    │                                            │
    │         ┌──────────────────────────────────┘
    │         │
    │    snapshot.jpg (JPEG)
    │         │
    │    ┌────┴────┐
    └───→│ describe│────→ llava (vision model)
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
