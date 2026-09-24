from __future__ import annotations

import asyncio
import json
import logging
import os
from datetime import datetime
from typing import Any

try:
    from google import genai
    from google.genai import types
except ImportError:  # pragma: no cover
    genai = None
    types = None

try:
    from .database import (
        append_conversation,
        build_system_context,
        fetch_recent_conversations,
        get_daily_events,
        get_daily_log,
        get_setting,
        list_routines,
        save_note,
        set_setting,
        upsert_daily_log,
    )
except ImportError:  # pragma: no cover
    from database import (
        append_conversation,
        build_system_context,
        fetch_recent_conversations,
        get_daily_events,
        get_daily_log,
        get_setting,
        list_routines,
        save_note,
        set_setting,
        upsert_daily_log,
    )

try:
    from .key_pool import ApiKeyPool
except ImportError:  # pragma: no cover
    from key_pool import ApiKeyPool

logger = logging.getLogger(__name__)


class GeminiKeyRotationError(RuntimeError):
    def __init__(self, message: str, failures: list[str]):
        super().__init__(message)
        self.failures = failures


class GeminiClient:
    def __init__(self, model: str = "gemini-2.0-flash") -> None:
        self.model_name = model
        self.key_pool = ApiKeyPool(
            [item.strip() for item in os.getenv("GEMINI_API_KEYS", "").split(",") if item.strip()],
            service="gemini",
        )

    def _schema(self, name: str, description: str, properties: dict[str, Any], required: list[str] | None = None) -> Any:
        if types is None:
            return {}
        return types.FunctionDeclaration(
            name=name,
            description=description,
            parameters=types.Schema(
                type="OBJECT",
                properties={k: types.Schema(type=v.get("type", "STRING"), description=v.get("description"), enum=v.get("enum"), minimum=v.get("minimum"), maximum=v.get("maximum")) for k, v in properties.items()},
                required=required or [],
            ),
        )

    def _tool_declarations(self) -> list[Any]:
        if types is None:
            return []
        return [
            self._schema("move_robot", "Move the robot in a direction", {"direction":{"type":"STRING","enum":["forward","backward","left","right"]},"speed":{"type":"INTEGER","minimum":0,"maximum":100},"duration_ms":{"type":"INTEGER","minimum":0,"maximum":20000}}, ["direction","speed","duration_ms"]),
            self._schema("stop_robot", "Stop the robot immediately", {}, []),
            self._schema("set_led", "Set the LED mode and color", {"mode":{"type":"STRING","enum":["on","off","blink","pulse"]},"color":{"type":"STRING"},"duration_ms":{"type":"INTEGER","minimum":0,"maximum":30000}}, ["mode","color"]),
            self._schema("set_alarm", "Set the alarm time", {"time":{"type":"STRING"}}, ["time"]),
            self._schema("add_routine", "Schedule a routine", {"time":{"type":"STRING"},"task":{"type":"STRING"}}, ["time","task"]),
            self._schema("remove_routine", "Remove a routine by time", {"time":{"type":"STRING"}}, ["time"]),
            self._schema("save_user_note", "Save personal information about someone", {"person":{"type":"STRING"},"info":{"type":"STRING"}}, ["person","info"]),
            self._schema("change_setting", "Update a robot setting", {"key":{"type":"STRING"},"value":{"type":"STRING"}}, ["key","value"]),
            self._schema("get_weather", "Get weather for a city", {"city":{"type":"STRING"}}, ["city"]),
            self._schema("start_web_manager", "Start the dashboard manager", {}, []),
            self._schema("stop_web_manager", "Stop the dashboard manager", {}, []),
            self._schema("play_radio", "Play a radio station", {"station_name":{"type":"STRING"}}, ["station_name"]),
            self._schema("set_volume", "Set speaker volume", {"level":{"type":"INTEGER","minimum":0,"maximum":100}}, ["level"]),
            self._schema("set_suspension", "Set the suspension pose or auto-level mode", {"mode":{"type":"STRING","enum":["stand_tall","crouch","level","auto_level_on","auto_level_off"]}}, ["mode"]),
            self._schema("turn_on_light", "Turn on a room light", {"room":{"type":"STRING"}}, ["room"]),
            self._schema("turn_off_light", "Turn off a room light", {"room":{"type":"STRING"}}, ["room"]),
            self._schema("set_reminder", "Set a reminder for a person", {"person":{"type":"STRING"},"time":{"type":"STRING"},"message":{"type":"STRING"}}, ["person","time","message"]),
            self._schema("forget_person", "Forget a person", {"person":{"type":"STRING"}}, ["person"]),
        ]

    def _config(self, *, use_tools: bool = False) -> Any | None:
        if types is None:
            return None
        cfg = {"temperature": 0.3, "system_instruction": build_system_context()}
        if use_tools:
            cfg["tools"] = [types.Tool(function_declarations=self._tool_declarations())]
        return types.GenerateContentConfig(**cfg)

    async def _call_with_rotation(self, prompt: str, *, use_tools: bool = False) -> Any:
        failures: list[str] = []
        for _ in range(max(1, len(self.key_pool.keys) if self.key_pool.keys else 1)):
            key = self.key_pool.next_key()
            if not key:
                break
            try:
                if genai is None:
                    raise RuntimeError("google-genai package is not installed")
                client = genai.Client(api_key=key)
                response = client.models.generate_content(
                    model=self.model_name,
                    contents=prompt,
                    config=self._config(use_tools=use_tools),
                )
                self.key_pool.mark_success(key)
                return response
            except Exception as exc:  # pragma: no cover
                failures.append(f"{key[:4]}...: {exc}")
                self.key_pool.mark_failure(key, str(exc))
                logger.warning("Gemini key failed: %s", exc)
        raise GeminiKeyRotationError("All configured Gemini keys failed; service unavailable.", failures)

    async def generate_reply(self, prompt: str) -> str:
        try:
            response = await asyncio.to_thread(self._generate_reply_sync, prompt)
            return response
        except GeminiKeyRotationError:
            return "I could not reach the Gemini service right now. Please try again." 

    def _generate_reply_sync(self, prompt: str) -> str:
        try:
            response = asyncio.run(self._call_with_rotation(prompt, use_tools=False))
            text = getattr(response, "text", None)
            if text:
                return str(text).strip() or "I am ready to help."
            for candidate in getattr(response, "candidates", []) or []:
                content = getattr(candidate, "content", None)
                if content is None:
                    continue
                parts = getattr(content, "parts", []) or []
                for part in parts:
                    value = getattr(part, "text", None)
                    if value:
                        return str(value).strip()
            return "I am ready to help."
        except Exception as exc:  # pragma: no cover
            logger.warning("Gemini reply generation failed: %s", exc)
            return "I could not reach the Gemini service right now. Please try again."

    async def handle_user_message(self, person: str, speaker: str, text: str) -> dict[str, Any]:
        append_conversation(person, speaker, text)
        reply = await self.generate_reply(text)
        append_conversation(person, "assistant", reply)
        return {"reply": reply, "intent": await self.classify_intent(text)}

    async def classify_intent(self, text: str) -> dict[str, Any]:
        lower = text.lower()
        if "stop" in lower or "නවත්තන්න" in lower:
            return {"intent": "emergency_stop", "confidence": 0.98}
        if "come" in lower or "ළඟ" in lower:
            return {"intent": "approach_user", "confidence": 0.94}
        return {"intent": "chat", "confidence": 0.8}

    async def parse_tool_calls(self, prompt: str) -> list[dict[str, Any]]:
        if not self.key_pool.keys:
            return []
        try:
            response = await self._call_with_rotation(prompt, use_tools=True)
            calls: list[dict[str, Any]] = []
            for candidate in getattr(response, "candidates", []) or []:
                for part in getattr(candidate.content, "parts", []) or []:
                    call = getattr(part, "function_call", None)
                    if call is not None:
                        args = getattr(call, "args", None) or {}
                        calls.append({"name": getattr(call, "name", ""), "arguments": dict(args)})
            return calls
        except GeminiKeyRotationError:
            return []

    def apply_tool_action(self, call: dict[str, Any]) -> dict[str, Any]:
        name = call.get("name")
        args = call.get("arguments", {})
        if name == "save_user_note":
            save_note(args.get("person", "default"), args.get("info", ""))
            return {"status": "ok", "action": "save_user_note"}
        if name == "add_routine":
            try:
                from .database import add_routine
            except ImportError:  # pragma: no cover
                from database import add_routine
            add_routine(args.get("time", "00:00"), args.get("task", ""))
            return {"status": "ok", "action": "add_routine"}
        if name == "remove_routine":
            try:
                from .database import remove_routine
            except ImportError:  # pragma: no cover
                from database import remove_routine
            remove_routine(args.get("time", "00:00"))
            return {"status": "ok", "action": "remove_routine"}
        if name == "change_setting":
            set_setting(args.get("key", ""), args.get("value", ""))
            return {"status": "ok", "action": "change_setting"}
        if name in {"move_robot", "set_led", "set_alarm", "set_volume", "set_suspension", "turn_on_light", "turn_off_light"}:
            return {"status": "ok", "action": name, "params": args}
        if name == "stop_robot":
            return {"status": "ok", "action": "stop_robot"}
        return {"status": "queued", "action": name, "params": args}

    def record_daily_history(self, date: str, summary: str) -> None:
        upsert_daily_log(date, summary)

    def current_state_snapshot(self) -> dict[str, Any]:
        today = datetime.utcnow().strftime("%Y-%m-%d")
        return {
            "ai_name": get_setting("ai_name", "Kaniye"),
            "language": get_setting("language", "Sinhala/English"),
            "mode": get_setting("mode", "helpful_companion"),
            "routines": list_routines(),
            "daily_log": get_daily_log(today),
            "daily_events": get_daily_events(today),
        }


ai_client = GeminiClient()
