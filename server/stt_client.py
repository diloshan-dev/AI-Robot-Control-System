from __future__ import annotations

import os

import requests

from key_pool import ApiKeyPool


class OpenAIWhisperSTT:
    def __init__(self, api_keys: list[str] | None = None) -> None:
        resolved = api_keys or [
            item.strip() for item in os.getenv("OPENAI_API_KEYS", os.getenv("OPENAI_API_KEY", "")).split(",") if item.strip()
        ]
        self.key_pool = ApiKeyPool(resolved, service="openai-whisper")
        self.endpoint = "https://api.openai.com/v1/audio/transcriptions"

    def transcribe(self, audio_bytes: bytes, filename: str = "input.wav", *, language: str | None = None) -> str:
        if not self.key_pool.keys:
            raise RuntimeError("OPENAI_API_KEYS is required for STT")

        last_error: Exception | None = None
        for _ in range(max(1, len(self.key_pool.keys))):
            api_key = self.key_pool.next_key()
            if not api_key:
                break
            try:
                files = {"file": (filename, audio_bytes, "audio/wav")}
                data = {"model": "whisper-1"}
                if language:
                    data["language"] = language
                # Intentionally omit language to allow Whisper auto-detect across Sinhala and English.
                headers = {"Authorization": f"Bearer {api_key}"}
                response = requests.post(self.endpoint, files=files, data=data, headers=headers, timeout=90)
                if response.status_code >= 400:
                    self.key_pool.mark_failure(api_key, f"status={response.status_code}")
                    last_error = RuntimeError(f"OpenAI STT error: {response.status_code} {response.text[:200]}")
                    continue
                self.key_pool.mark_success(api_key)
                payload = response.json()
                return payload.get("text", "")
            except Exception as exc:  # pragma: no cover - network-dependent path
                self.key_pool.mark_failure(api_key, str(exc))
                last_error = exc
                continue

        if last_error is not None:
            raise RuntimeError(f"OpenAI Whisper STT failed for all configured keys: {last_error}")
        raise RuntimeError("OpenAI Whisper STT failed: no configured API keys")
