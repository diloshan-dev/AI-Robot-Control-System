from __future__ import annotations

import asyncio
import base64
import json
from datetime import datetime
from typing import Any

import uvicorn
from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse
from pydantic import BaseModel, Field

try:
    from .ai_client import GeminiClient
    from .config import APP_NAME, GEMINI_MODEL, HOST, PORT
    from .database import (
        default_settings,
        fetch_recent_conversations,
        get_daily_log,
        get_setting,
        init_db,
        list_routines,
        personal_notes_snapshot,
        save_daily_log,
    )
    from .stt_client import OpenAIWhisperSTT
    from .tts_client import ElevenLabsTTS
except ImportError:  # pragma: no cover - allows running this file directly
    from ai_client import GeminiClient
    from config import APP_NAME, GEMINI_MODEL, HOST, PORT
    from database import (
        default_settings,
        fetch_recent_conversations,
        get_daily_log,
        get_setting,
        init_db,
        list_routines,
        personal_notes_snapshot,
        save_daily_log,
    )
    from stt_client import OpenAIWhisperSTT
    from tts_client import ElevenLabsTTS

app = FastAPI(title=APP_NAME)
init_db()
default_settings()
ai_client = GeminiClient(model=GEMINI_MODEL)

tts_client = ElevenLabsTTS()
stt_client = OpenAIWhisperSTT()


class RobotCommand(BaseModel):
    action: str = Field(..., examples=["move", "stop", "speak", "listen", "follow"])
    direction: str | None = None
    speed: int = Field(default=60, ge=0, le=100)
    payload: str | None = None


class ChatRequest(BaseModel):
    prompt: str
    person: str = "default"
    speaker: str = "user"
    volume_hint: int = 50


@app.get("/health")
async def health() -> dict[str, Any]:
    return {
        "status": "ok",
        "app": APP_NAME,
        "ai": "gemini-ready",
        "features": [
            "voice",
            "vision",
            "memory",
            "home_automation",
            "robot_control",
            "dashboard",
        ],
    }


@app.get("/keys/status")
async def keys_status() -> dict[str, Any]:
    return {
        "gemini": ai_client.key_pool.status(),
        "elevenlabs": tts_client.key_pool.status(),
        "stt": stt_client.key_pool.status(),
    }


@app.post("/command")
async def command(payload: RobotCommand) -> dict[str, Any]:
    if payload.action == "stop":
        return {"status": "queued", "action": "stop", "note": "Emergency stop behavior activated"}

    if payload.action == "move":
        return {
            "status": "queued",
            "action": "move",
            "direction": payload.direction or "forward",
            "speed": payload.speed,
        }

    if payload.action == "speak":
        return {"status": "queued", "action": "speak", "text": payload.payload or "Hello"}

    return {"status": "queued", "action": payload.action, "payload": payload.payload}


@app.post("/chat")
async def chat(payload: ChatRequest) -> dict[str, Any]:
    try:
        result = await ai_client.handle_user_message(payload.person, payload.speaker, payload.prompt)
        result["volume_hint"] = payload.volume_hint
        return result
    except Exception as exc:  # pragma: no cover - network dependent
        return {"reply": f"I hit a problem: {exc}", "intent": {"intent": "error", "confidence": 0.0}}


@app.get("/history/{log_date}")
async def history(log_date: str) -> dict[str, Any]:
    log_summary = get_daily_log(log_date)
    return {
        "date": log_date,
        "summary": log_summary,
        "conversation": fetch_recent_conversations("default", 100),
    }


@app.get("/dashboard", response_class=HTMLResponse)
async def dashboard() -> HTMLResponse:
    settings = {
        "ai_name": get_setting("ai_name", "Kaniye"),
        "language": get_setting("language", "Sinhala/English"),
        "mode": get_setting("mode", "helpful_companion"),
        "word_limit": get_setting("word_limit", "150"),
    }
    notes = personal_notes_snapshot()
    routines = list_routines()
    today = datetime.utcnow().strftime("%Y-%m-%d")
    summary = get_daily_log(today) or "No summary logged yet."
    page = f"""
    <html><head><title>{APP_NAME} Dashboard</title></head>
    <body style="font-family:Arial;padding:24px;">
      <h1>{APP_NAME}</h1>
      <h2>Settings</h2><pre>{json.dumps(settings, ensure_ascii=False, indent=2)}</pre>
      <h2>Notes</h2><pre>{json.dumps(notes, ensure_ascii=False, indent=2)}</pre>
      <h2>Routines</h2><pre>{json.dumps(routines, ensure_ascii=False, indent=2)}</pre>
      <h2>Daily log</h2><pre>{summary}</pre>
    </body></html>
    """
    return HTMLResponse(page)


@app.websocket("/ws/robot")
async def robot_ws(websocket: WebSocket) -> None:
    await websocket.accept()
    try:
        while True:
            message = await websocket.receive_text()
            payload = json.loads(message)
            msg_type = payload.get("type")

            if msg_type == "telemetry":
                await websocket.send_text(json.dumps({"type": "ack", "status": "ok", "received": msg_type}))
                continue

            if msg_type == "audio_chunk":
                data = payload.get("data", "")
                try:
                    audio_bytes = base64.b64decode(data)
                except Exception:
                    audio_bytes = b""
                transcription = ""
                if audio_bytes:
                    try:
                        transcription = stt_client.transcribe(audio_bytes)
                    except Exception as exc:
                        transcription = f"STT error: {exc}"
                response = await ai_client.generate_reply(transcription or "silence")
                try:
                    audio = tts_client.synthesize(response, volume_hint=int(payload.get("volume_hint", 50)))
                    payload_out = {
                        "type": "audio_response",
                        "data": base64.b64encode(audio).decode("utf-8"),
                        "volume_hint": int(payload.get("volume_hint", 50)),
                        "text": response,
                    }
                    await websocket.send_text(json.dumps(payload_out))
                except Exception as exc:
                    await websocket.send_text(json.dumps({"type": "error", "message": str(exc)}))
                continue

            if msg_type == "event":
                name = payload.get("name", "unknown")
                save_daily_log(datetime.utcnow().strftime("%Y-%m-%d"), f"Event: {name}")
                await websocket.send_text(json.dumps({"type": "ack", "status": "ok", "event": name}))
                continue

            tool_calls = await ai_client.parse_tool_calls(json.dumps(payload))
            for call in tool_calls:
                action = ai_client.apply_tool_action(call)
                await websocket.send_text(json.dumps({"type": "command", "action": action.get("action"), "params": action.get("params", {})}))

            await websocket.send_text(json.dumps({"type": "ack", "status": "ok", "received": payload}))
    except WebSocketDisconnect:
        return
    except Exception:
        return


if __name__ == "__main__":
    uvicorn.run("robot_server:app", host=HOST, port=PORT, reload=False)
