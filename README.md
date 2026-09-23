# Kaniye Companion Robot

Kaniye is a Sinhala/English AI companion robot system built as a split architecture:

- ESP32 firmware uses pure ESP-IDF and runs local safety logic in FreeRTOS.
- Python server handles Gemini reasoning, ElevenLabs TTS, Whisper STT, database-backed memory, and real command orchestration.

## Project structure

- `firmware/robot_esp32` — ESP-IDF firmware with local safety/motor logic and WiFi + WebSocket communication.
- `server` — FastAPI App for AI orchestration, audio processing, and dashboard endpoints.
- `.env.example` — required environment keys and runtime config template.

## Quick setup

1. Create a `.env` file in the project root by copying `.env.example`.
2. Add your API keys:
   - `GEMINI_API_KEYS`
   - `ELEVENLABS_API_KEY`
   - `ELEVENLABS_VOICE_ID`
   - `OPENAI_API_KEY`
3. Install Python dependencies:

```bash
cd server
python -m venv .venv
. .venv\Scripts\activate
pip install -r requirements.txt
```

4. Launch the server:

```bash
python robot_server.py
```

5. Configure the ESP32 WiFi values through `idf.py menuconfig` and set:
   - `Kaniye Robot Settings -> WiFi SSID`
   - `Kaniye Robot Settings -> WiFi Password`
   - `Kaniye Robot Settings -> Robot server host`
   - `Kaniye Robot Settings -> Robot server port`

6. Build and flash firmware:

```bash
cd firmware/robot_esp32
idf.py build
idf.py flash monitor
```

## AI / API choices

- Gemini: `google-generativeai` with multi-key rotation, automatic fallback, and structured function calling.
- ElevenLabs: REST API synthesize final response audio and send it back over the WebSocket with a `volume_hint`.
- STT: OpenAI Whisper API (`OPENAI_API_KEY`). Whisper supports many languages, including Sinhala in practice, but recognition quality depends on the audio quality and accent.

## ESP32 ↔ Server message schema

ESP32 -> Server:

```json
{"type":"telemetry","battery_pct":87.5,"sensors":{"temperature":30.2}}
{"type":"audio_chunk","data":"base64_payload","seq":12}
{"type":"event","name":"emergency_stop"}
```

Server -> ESP32:

```json
{"type":"command","action":"move","params":{"direction":"forward","speed":60}}
{"type":"command","action":"stop","params":{}}
{"type":"audio_response","data":"base64_audio","volume_hint":80}
```

## Database-backed features

The server stores:

- settings
- personal notes
- routines
- conversation history
- daily logs

all in SQLite via `server/database.py` instead of SD-card files.

## Local safety rules

The ESP32 keeps emergency-stop and watchdog safety logic local to the device, so even when WiFi or the PC server is unavailable, the robot still stops immediately on hardware-triggered safety events.

## Notes

- `.env` is ignored by git and must never be committed.
- No hardcoded secrets are stored in source files.
- The old Arduino prototype behavior is modernized into structured tool-calling and database-backed memory rather than direct copied sketch logic.
