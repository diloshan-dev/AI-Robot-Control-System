from __future__ import annotations

import asyncio
import json
from typing import Any

import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from pydantic import BaseModel, Field

try:
    from .ai_client import GeminiClient
    from .config import APP_NAME, GEMINI_MODEL, HOST, PORT
except ImportError:  # pragma: no cover - allows running this file directly
    from ai_client import GeminiClient
    from config import APP_NAME, GEMINI_MODEL, HOST, PORT

app = FastAPI(title=APP_NAME)
ai_client = GeminiClient(model=GEMINI_MODEL)


class RobotCommand(BaseModel):
    action: str = Field(..., examples=["move", "stop", "speak", "listen", "follow"])
    direction: str | None = None
    speed: int = Field(default=60, ge=0, le=100)
    payload: str | None = None


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
        ],
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
async def chat(prompt: str) -> dict[str, Any]:
    intent = await ai_client.classify_intent(prompt)
    reply = await ai_client.generate_reply(prompt)
    return {"intent": intent, "reply": reply}


@app.websocket("/ws/robot")
async def robot_ws(websocket: WebSocket) -> None:
    await websocket.accept()
    try:
        while True:
            message = await websocket.receive_text()
            data = json.loads(message)
            response = {
                "received": data,
                "status": "ok",
                "timestamp": asyncio.get_running_loop().time(),
            }
            await websocket.send_text(json.dumps(response))
    except WebSocketDisconnect:
        return


if __name__ == "__main__":
    uvicorn.run("robot_server:app", host=HOST, port=PORT, reload=False)
