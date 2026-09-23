from __future__ import annotations

import base64
import os
from typing import Any

import requests


class ElevenLabsTTS:
    def __init__(self, api_key: str | None = None, voice_id: str | None = None) -> None:
        self.api_key = api_key or os.getenv("ELEVENLABS_API_KEY")
        self.voice_id = voice_id or os.getenv("ELEVENLABS_VOICE_ID")
        self.base_url = "https://api.elevenlabs.io/v1"

    def synthesize(self, text: str, *, volume_hint: int = 50) -> bytes:
        if not self.api_key or not self.voice_id:
            raise RuntimeError("ElevenLabs API key and Voice ID are required")

        url = f"{self.base_url}/text-to-speech/{self.voice_id}"
        payload = {
            "text": text,
            "model_id": "eleven_multilingual_v2",
            "voice_settings": {
                "stability": 0.5,
                "similarity_boost": 0.72,
                "volume": max(0.0, min(1.0, volume_hint / 100.0)),
            },
        }
        headers = {
            "Content-Type": "application/json",
            "xi-api-key": self.api_key,
        }
        response = requests.post(url, json=payload, headers=headers, timeout=60)
        if response.status_code >= 400:
            raise RuntimeError(f"ElevenLabs TTS error: {response.status_code} {response.text[:200]}")
        return response.content

    def synthesize_b64(self, text: str, *, volume_hint: int = 50) -> str:
        return base64.b64encode(self.synthesize(text, volume_hint=volume_hint)).decode("utf-8")
