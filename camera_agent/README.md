# Camera Agent

A managed AI agent that controls an ESP32-S3 BLE camera using natural language. When you ask it to take a picture, the LLM calls the `take_picture` tool, which connects to the ESP32-CAM over BLE and captures a JPEG snapshot. Ask it to describe the photo and it calls `describe_picture`, which hands the JPEG to a local vision model.

## Architecture

Two models are used, because the model that is good at conversation and tool calling is not the model that can see:

```
User: "take a picture and then describe it"
   │
   ▼
gpt-oss:20b  (CAMERA_AGENT_MODEL)
   ManagedAgent() → .with_model(ModelConfig(...))
   │
   ├── take_picture()      ──BLE──►  ESP32-CAM       ──►  snapshot.jpg
   │                                                       (returns path + byte size)
   │
   └── describe_picture("snapshot.jpg")
            │
            └── llava  (CAMERA_AGENT_VISION_MODEL)
                ManagedAgent() → .run([text, ImageUrl], ...)
                via Ollama /v1/chat/completions
                │
                └──►  description text
```

Both the main agent and the vision agent are built with the same `ManagedAgent` / `ModelConfig` / `build_model()` factory pattern from `agent_harness`. The only difference is the prompt type: the main agent gets a plain string, the vision agent gets a multimodal list (`[str, ImageUrl]`). The main model never receives image pixels, so it is instructed to relay `describe_picture`'s output verbatim rather than describe anything itself.

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

### 3. Pull the vision model

Image description is served by Ollama regardless of which provider runs the main
model, so Ollama must be running and the vision model must be present:

```bash
ollama pull llava
```

### 4. Install dependencies

```bash
uv sync
```

This clones and installs both dependencies from GitHub:
- `agent-harness` — the managed agent framework
- `embedded-system-services` — the BLE camera client library

### 5. Run the agent

```bash
uv run python -m camera_agent.main
```

## Usage

```
> Take a picture
Photo saved to snapshot.jpg (4521 bytes)

> Take a photo and save it as kitchen.jpg
Photo saved to kitchen.jpg (4490 bytes)

> Take a picture and then describe it
The image shows an interior space that appears to be a room with minimal
furnishings... There are window blinds partially closed, allowing natural
light to enter the room.

> What colour are the walls in snapshot.jpg?
The image is too blurry to discern specific details, including the color of
the walls.

> exit
```

The agent understands natural language — you can phrase the request however you want. Any prompt that asks for a photo triggers `take_picture`; any prompt asking about image content triggers `describe_picture`. A specific question is passed through to the vision model rather than answered from the description.

Asking about a file that already exists does **not** re-capture. This is deliberate: `take_picture` overwrites its target, so retaking on a question would destroy the image being asked about. The system prompt forbids it.

Note the second answer above — a refusal rather than a guess. That is the correct outcome, and preferable to the confident fiction a text-only model produces.

## Limitations

### Descriptions are only as good as the vision model

`llava` is a small local model reading a 320x240 JPEG from a low-cost sensor. It reliably reports scene-level facts — indoor/outdoor, wall colour, lighting, blinds, whether a space looks occupied — but it misidentifies small objects, and it will label a dark shape on the floor a "portable stove" with the same confidence it describes a wall. Treat object-level claims as guesses.

Raising quality is mostly a firmware-side problem, not a prompt problem: increase frame size and JPEG quality in the CameraBLE config so the vision model has more pixels to work with.

### The main model still can't see

`take_picture` returns only a path and byte size, and `describe_picture` returns text. Neither puts pixels into the main model's context. The system prompt therefore instructs the main model to relay `describe_picture` output verbatim.

This matters: an earlier version let the main model summarise the description in its own words, and `gpt-oss:20b` inverted "bright light source" into "dark indoor space", invented a floor colour that `llava` had explicitly called not visible, and turned one uncertain door into two. If you edit the prompt in `src/camera_agent/agent_factory.py`, keep the verbatim instruction, and keep the prompt short — a long multi-rule prompt made the 20B model emit raw tool-call JSON as its answer instead of calling the tool.

### Swapping the main model does not change the vision path

`describe_picture` always routes through Ollama (via `ManagedAgent` + `ModelConfig` + `build_model()`). Setting `CAMERA_AGENT_PROVIDER=openai` changes the conversational model only; description still runs through `CAMERA_AGENT_VISION_MODEL` on local Ollama. Vision model options:

| Model | Notes |
|---|---|
| `llava` | Default. 4.7GB, local, no API key. |
| `llava-phi3` | Smaller and faster, weaker descriptions. |

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| Agent describes objects that are not in the photo | Small vision model misidentifying detail in a low-res frame. | Expected for object-level claims. Raise frame size/quality in the CameraBLE firmware. |
| Agent's description contradicts the image, e.g. calls a bright room dark | Main model paraphrasing instead of relaying `describe_picture` verbatim. | Restore the verbatim instruction in the system prompt in `src/camera_agent/agent_factory.py`. |
| Agent prints raw JSON such as `{"output_path":"snapshot.jpg"}` and never calls the tool | System prompt too long/complex for the main model, so it emits a tool call as text. | Shorten the system prompt. |
| Asking a question about an image silently replaces it with a new capture | Main model calling `take_picture` before `describe_picture` on a describe-only request. | Restore the "never call take_picture in this case" line in the system prompt. |
| `Vision model 'llava' failed: All 3 retries exhausted. Last error: Connection error.` | Ollama not running, or listening elsewhere. | Start Ollama, or set `OLLAMA_BASE_URL`. |
| `Vision model 'llava' returned 404. Confirm the model is installed...` | Vision model not pulled. | `ollama pull llava` |
| `Scanning for 'ESP32-CAM' ...` then timeout | Board not powered, not flashed, or advertising under a different name. | Confirm the CameraBLE firmware is running; check the serial log for the advertised name and set `CAMERA_AGENT_DEVICE` to match. |
| Connects but capture hangs or truncates | Frame notifications are not subscribed, usually a missing CCCD on the frame characteristic. | Verify `BLE2902` is attached to the frame characteristic in `CameraBluetoothServer.cpp`. |
| macOS pairs to the wrong device | Multiple boards advertising the same name. | Set `CAMERA_AGENT_DEVICE` to a unique name and reflash. |

## Configuration

| Env Variable | Default | Description |
|---|---|---|
| `CAMERA_AGENT_PROVIDER` | `ollama` | LLM provider. See supported providers in the pydanticai-fluent USAGE.md. |
| `CAMERA_AGENT_MODEL` | `gpt-oss:20b` | Main model, used for conversation and tool calling. |
| `CAMERA_AGENT_VISION_MODEL` | `llava` | Vision model used by `describe_picture`. Always served by Ollama. |
| `CAMERA_AGENT_DEVICE` | `ESP32-CAM` | BLE device name to scan for. Change if your board advertises differently. |
| `OLLAMA_BASE_URL` | `http://localhost:11434` | Ollama endpoint. A trailing `/v1` is stripped for the vision call. |
| `CAMERA_AGENT_DEBUG` | `0` | Set to `1` to print every LLM tool-call decision and tool execution trace. |

## Debug mode

Set `CAMERA_AGENT_DEBUG=1` to see exactly what each tool does:

```
> take a picture and describe it
🔧  [take_picture] start  args={'output_path': 'snapshot.jpg'}
Scanning for 'ESP32-CAM' ...
Found: ESP32-CAM
Connected: True
Frame saved: snapshot.jpg (4963 bytes)
🔧  [take_picture] end  success=True  duration=2.341s
🔧  [describe_picture] start  args={'image_path': 'snapshot.jpg', 'question': ''}
🔧  [describe_picture] end  success=True  duration=7.192s
The image shows a room with a sloped white ceiling...
```

This is useful for:
- Seeing what arguments the LLM passes to each tool
- Measuring how long BLE capture vs vision inference take
- Diagnosing why a tool was skipped or failed
