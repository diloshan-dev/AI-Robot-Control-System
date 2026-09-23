from __future__ import annotations

import os
from typing import Any

import requests


class OpenAIWhisperSTT:
    def __init__(self, api_key: str | None = None) -> None:
        self.api_key = api_key or os.getenv("OPENAI_API_KEY")
        self.endpoint = "https://api.openai.com/v1/audio/transcriptions"

    def transcribe(self, audio_bytes: bytes, filename: str = "input.wav") -> str:
        if not self.api_key:
            raise RuntimeError("OPENAI_API_KEY is required for STT")

        files = {"file": (filename, audio_bytes, "audio/wav")}
        data = {"model": "whisper-1", "language": "si"}
        headers = {"Authorization": f"Bearer {self.api_key}"}
        response = requests.post(self.endpoint, files=files, data=data, headers=headers, timeout=90)
        if response.status_code >= 400:
            raise RuntimeError(f"OpenAI STT error: {response.status_code} {response.text[:200]}")
        payload = response.json()
        return payload.get("text", "")
