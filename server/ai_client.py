from __future__ import annotations

import asyncio
from typing import Any


class GeminiClient:
    """Thin wrapper around the Gemini AI API. Replace with real SDK integration later."""

    def __init__(self, model: str = "gemini-2.0-flash") -> None:
        self.model = model

    async def generate_reply(self, text: str, *, vision_context: str | None = None) -> str:
        await asyncio.sleep(0.05)

        if vision_context:
            return (
                f"[Gemini Vision] Context: {vision_context}. "
                f"User prompt: {text}. This response is a placeholder for the real Gemini integration."
            )

        return (
            f"[Gemini] I received: '{text}'. "
            "The real AI answer will be generated from the Gemini API in production."
        )

    async def classify_intent(self, text: str) -> dict[str, Any]:
        lowered = text.lower()
        if "stop" in lowered or "නවත්තන්න" in lowered:
            return {"intent": "emergency_stop", "confidence": 0.98}
        if "come" in lowered or "ළඟ" in lowered:
            return {"intent": "approach_user", "confidence": 0.94}
        return {"intent": "chat", "confidence": 0.80}
