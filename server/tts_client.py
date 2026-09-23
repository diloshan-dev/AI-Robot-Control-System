from __future__ import annotations

import base64
import os
from typing import Any

import requests

from key_pool import ApiKeyPool


class ElevenLabsTTS:
    def __init__(self, api_keys: list[str] | None = None, voice_id: str | None = None) -> None:
        resolved = api_keys or [
            item.strip() for item in os.getenv("ELEVENLABS_API_KEYS", os.getenv("ELEVENLABS_API_KEY", "")).split(",") if item.strip()
        ]
        self.key_pool = ApiKeyPool(resolved, service="elevenlabs")
        self.voice_id = voice_id or os.getenv("ELEVENLABS_VOICE_ID")
        self.base_url = "https://api.elevenlabs.io/v1"

    def synthesize(self, text: str, *, volume_hint: int = 50) -> bytes:
        if not self.voice_id:
            raise RuntimeError("ELEVENLABS_VOICE_ID is required")

        last_error: Exception | None = None
        for _ in range(max(1, len(self.key_pool.keys))):
            api_key = self.key_pool.next_key()
            if not api_key:
                break
            try:
                url = f"{self.base_url}/text-to-speech/{self.voice_id}"
                payload = {
                    "text": text,
                    "model_id": "eleven_multilingual_v2",
                    "voice_settings": {
                        "stability": 0.5,
                        "similarity_boost": 0.72,
                    },
                }
                headers = {
                    "Content-Type": "application/json",
                    "xi-api-key": api_key,
                }
                response = requests.post(url, json=payload, headers=headers, timeout=60)
                if response.status_code >= 400:
                    self.key_pool.mark_failure(api_key, f"status={response.status_code}")
                    last_error = RuntimeError(f"ElevenLabs TTS error: {response.status_code} {response.text[:200]}")
                    continue
                self.key_pool.mark_success(api_key)
                return response.content
            except Exception as exc:  # pragma: no cover - network-dependent path
                self.key_pool.mark_failure(api_key, str(exc))
                last_error = exc
                continue

        if last_error is not None:
            raise RuntimeError(f"ElevenLabs TTS failed for all configured keys: {last_error}")
        raise RuntimeError("ElevenLabs TTS failed: no configured API keys")

    def synthesize_b64(self, text: str, *, volume_hint: int = 50) -> str:
        return base64.b64encode(self.synthesize(text, volume_hint=volume_hint)).decode("utf-8")
