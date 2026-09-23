from __future__ import annotations

import os
from pathlib import Path

from dotenv import load_dotenv

load_dotenv(Path(__file__).resolve().parent.parent / ".env", override=False)

APP_NAME = "Kaniye Companion Robot"
HOST = os.getenv("ROBOT_SERVER_HOST", "0.0.0.0")
PORT = int(os.getenv("ROBOT_SERVER_PORT", "8000"))
GEMINI_MODEL = "gemini-2.0-flash"
AI_SERVER_TIMEOUT_SECONDS = 30

GEMINI_API_KEYS = [
    key.strip() for key in os.getenv("GEMINI_API_KEYS", "").split(",") if key.strip()
]
ELEVENLABS_API_KEY = os.getenv("ELEVENLABS_API_KEY", "")
ELEVENLABS_VOICE_ID = os.getenv("ELEVENLABS_VOICE_ID", "")
OPENAI_API_KEY = os.getenv("OPENAI_API_KEY", "")

VOICE_KEYWORDS = {
    "stop": ["නවත්තන්න", "STOP", "stop"],
    "come_here": ["ළඟට වා", "come here", "come_here"],
    "unknown": ["ඇහුනේ නෑ", "තේරුනේ නෑ"],
}
