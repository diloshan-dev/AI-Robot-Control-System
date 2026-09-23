from __future__ import annotations

import asyncio
import logging
import os
from datetime import datetime
from typing import Any

try:
    import google.generativeai as genai
except ImportError:  # pragma: no cover - installed in the runtime environment
    genai = None

try:
    from .database import (
        append_conversation,
        build_system_context,
        fetch_recent_conversations,
        get_daily_log,
        get_setting,
        list_routines,
        save_note,
        serialize_json,
        set_setting,
        upsert_daily_log,
    )
except ImportError:  # pragma: no cover - direct script execution
    from database import (
        append_conversation,
        build_system_context,
        fetch_recent_conversations,
        get_daily_log,
        get_setting,
        list_routines,
        save_note,
        serialize_json,
        set_setting,
        upsert_daily_log,
    )

logger = logging.getLogger(__name__)


class GeminiKeyRotationError(RuntimeError):
    def __init__(self, message: str, failures: list[str]):
        super().__init__(message)
        self.failures = failures


class GeminiKeyManager:
    def __init__(self) -> None:
        configured = os.getenv("GEMINI_API_KEYS", "")
        self.keys: list[str] = [key.strip() for key in configured.split(",") if key.strip()]
        if not self.keys:
            logger.warning("No Gemini API keys configured. Gemini calls will fail until keys are added.")

    def rotate_key(self) -> str | None:
        if not self.keys:
            return None
        key = self.keys.pop(0)
        self.keys.append(key)
        return key

    def current_key(self) -> str | None:
        return self.keys[0] if self.keys else None


class GeminiClient:
    """Real Gemini-backed AI client with multi-key fallback and tool-calling support."""

    TOOL_SCHEMAS = [
        {
            "type": "function",
            "function": {
                "name": "move_robot",
                "description": "Move the robot in a direction at a given speed for a fixed duration.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "direction": {"type": "string", "enum": ["forward", "backward", "left", "right"]},
                        "speed": {"type": "integer", "minimum": 0, "maximum": 100},
                        "duration_ms": {"type": "integer", "minimum": 0, "maximum": 20000},
                    },
                    "required": ["direction", "speed", "duration_ms"],
                },
            },
        },
        {
            "type": "function",
            "function": {
                "name": "stop_robot",
                "description": "Stop the robot immediately.",
                "parameters": {"type": "object", "properties": {}},
            },
        },
        {
            "type": "function",
            "function": {
                "name": "set_led",
                "description": "Set a LED mode, color and duration.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "mode": {"type": "string", "enum": ["on", "off", "blink", "pulse"]},
                        "color": {"type": "string"},
                        "duration_ms": {"type": "integer", "minimum": 0, "maximum": 30000},
                    },
                    "required": ["mode", "color"],
                },
            },
        },
        {
            "type": "function",
            "function": {
                "name": "set_alarm",
                "description": "Set an alarm time in HH:MM format.",
                "parameters": {"type": "object", "properties": {"time": {"type": "string"}}, "required": ["time"]},
            },
        },
        {
            "type": "function",
            "function": {
                "name": "add_routine",
                "description": "Add a scheduled routine task.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "time": {"type": "string"},
                        "task": {"type": "string"},
                    },
                    "required": ["time", "task"],
                },
            },
        },
        {
            "type": "function",
            "function": {
                "name": "remove_routine",
                "description": "Remove a routine by the scheduled time.",
                "parameters": {"type": "object", "properties": {"time": {"type": "string"}}, "required": ["time"]},
            },
        },
        {
            "type": "function",
            "function": {
                "name": "save_user_note",
                "description": "Store user personal information for future context.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "person": {"type": "string"},
                        "info": {"type": "string"},
                    },
                    "required": ["person", "info"],
                },
            },
        },
        {
            "type": "function",
            "function": {
                "name": "change_setting",
                "description": "Update a robot setting.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "key": {"type": "string"},
                        "value": {"type": "string"},
                    },
                    "required": ["key", "value"],
                },
            },
        },
        {
            "type": "function",
            "function": {
                "name": "get_weather",
                "description": "Check weather for a city.",
                "parameters": {"type": "object", "properties": {"city": {"type": "string"}}, "required": ["city"]},
            },
        },
        {
            "type": "function",
            "function": {
                "name": "start_web_manager",
                "description": "Start the web manager dashboard from the server.",
                "parameters": {"type": "object", "properties": {}},
            },
        },
        {
            "type": "function",
            "function": {
                "name": "stop_web_manager",
                "description": "Stop the web manager dashboard.",
                "parameters": {"type": "object", "properties": {}},
            },
        },
        {
            "type": "function",
            "function": {
                "name": "play_radio",
                "description": "Play a radio station via the robot media subsystem.",
                "parameters": {"type": "object", "properties": {"station_name": {"type": "string"}}, "required": ["station_name"]},
            },
        },
        {
            "type": "function",
            "function": {
                "name": "set_volume",
                "description": "Set the spoken audio volume level.",
                "parameters": {"type": "object", "properties": {"level": {"type": "integer", "minimum": 0, "maximum": 100}}, "required": ["level"]},
            },
        },
        {
            "type": "function",
            "function": {
                "name": "turn_on_light",
                "description": "Turn on a light in a room.",
                "parameters": {"type": "object", "properties": {"room": {"type": "string"}}, "required": ["room"]},
            },
        },
        {
            "type": "function",
            "function": {
                "name": "turn_off_light",
                "description": "Turn off a light in a room.",
                "parameters": {"type": "object", "properties": {"room": {"type": "string"}}, "required": ["room"]},
            },
        },
        {
            "type": "function",
            "function": {
                "name": "set_reminder",
                "description": "Set a timed reminder for a person.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "person": {"type": "string"},
                        "time": {"type": "string"},
                        "message": {"type": "string"},
                    },
                    "required": ["person", "time", "message"],
                },
            },
        },
        {
            "type": "function",
            "function": {
                "name": "forget_person",
                "description": "Remove a person from known memory.",
                "parameters": {"type": "object", "properties": {"person": {"type": "string"}}, "required": ["person"]},
            },
        },
    ]

    def __init__(self, model: str = "gemini-2.0-flash") -> None:
        self.model_name = model
        self.key_manager = GeminiKeyManager()
        self._model = None
        self._last_failure: str | None = None
        if genai is not None and self.key_manager.current_key():
            genai.configure(api_key=self.key_manager.current_key())
            self._model = genai.GenerativeModel(self.model_name)

    def _ensure_model(self) -> Any:
        if genai is None:
            raise RuntimeError("google-generativeai package is not installed")
        key = self.key_manager.current_key()
        if not key:
            raise RuntimeError("No Gemini API key configured")
        genai.configure(api_key=key)
        self._model = genai.GenerativeModel(self.model_name)
        return self._model

    async def _call_with_rotation(self, prompt: str, *, system_prompt: str | None = None, function_calls: bool = True) -> Any:
        failures: list[str] = []
        for _ in range(max(1, len(self.key_manager.keys) or 1)):
            key = self.key_manager.rotate_key()
            if not key:
                break
            try:
                model = self._ensure_model()
                if function_calls:
                    return model.generate_content(
                        [system_prompt or build_system_context(), prompt],
                        tools=self.TOOL_SCHEMAS,
                        generation_config={"temperature": 0.4},
                    )
                return model.generate_content([system_prompt or build_system_context(), prompt])
            except Exception as exc:  # pragma: no cover - depends on network/api availability
                failure = f"key={key[:4]}... ({exc})"
                failures.append(failure)
                logger.warning("Gemini key failed: %s", failure)
                self._last_failure = str(exc)
        if failures:
            raise GeminiKeyRotationError("All Gemini API keys failed or no keys were configured.", failures)
        raise RuntimeError("No Gemini API keys available")

    async def generate_reply(self, text: str, *, vision_context: str | None = None) -> str:
        system_prompt = build_system_context()
        prompt = text if not vision_context else f"Vision context: {vision_context}\nUser prompt: {text}"
        response = await asyncio.to_thread(self._generate_reply_sync, prompt, system_prompt)
        return response

    def _generate_reply_sync(self, prompt: str, system_prompt: str) -> str:
        if genai is None:
            return f"Gemini SDK unavailable. User message: {prompt}"
        try:
            response = asyncio.run(self._call_with_rotation(prompt, system_prompt=system_prompt, function_calls=False))
            result = getattr(response, "text", None)
            if result is None:
                result = response.candidates[0].content.parts[0].text
            return result.strip() or "I am here and ready to help."
        except Exception:
            return "I could not reach the Gemini service right now. Please try again in a moment."

    async def classify_intent(self, text: str) -> dict[str, Any]:
        lowered = text.lower()
        if "stop" in lowered or "නවත්තන්න" in lowered:
            return {"intent": "emergency_stop", "confidence": 0.98}
        if "come" in lowered or "ළඟ" in lowered:
            return {"intent": "approach_user", "confidence": 0.94}
        return {"intent": "chat", "confidence": 0.80}

    async def handle_user_message(self, person: str, speaker: str, text: str) -> dict[str, Any]:
        append_conversation(person, speaker, text)
        response_text = await self.generate_reply(text)
        append_conversation(person, "assistant", response_text)
        return {"reply": response_text, "intent": await self.classify_intent(text)}

    async def summarize_history(self, person: str) -> str:
        history = fetch_recent_conversations(person, 80)
        transcript = "\n".join(f"{row['speaker']}: {row['message']}" for row in history)
        if not transcript:
            return ""
        try:
            response = await self._call_with_rotation(
                "Summarize the conversation into a short memory snapshot for a robot companion.\n" + transcript,
                system_prompt=build_system_context(),
                function_calls=False,
            )
            return getattr(response, "text", "").strip() or transcript[:1000]
        except Exception as exc:  # pragma: no cover - network dependent
            logger.warning("Conversation summarization failed: %s", exc)
            return transcript[:1000]

    async def parse_tool_calls(self, prompt: str) -> list[dict[str, Any]]:
        response = await self._call_with_rotation(prompt, system_prompt=build_system_context(), function_calls=True)
        calls: list[dict[str, Any]] = []
        try:
            for part in response.candidates[0].content.parts:
                call = getattr(part, "function_call", None)
                if call is not None:
                    calls.append({"name": call.name, "arguments": dict(call.args)})
        except Exception:
            pass
        return calls

    def record_daily_history(self, date: str, summary: str) -> None:
        upsert_daily_log(date, summary)

    def current_state_snapshot(self) -> dict[str, Any]:
        return {
            "ai_name": get_setting("ai_name", "Kaniye"),
            "language": get_setting("language", "Sinhala/English"),
            "mode": get_setting("mode", "helpful_companion"),
            "routines": list_routines(),
            "daily_log": get_daily_log(datetime.utcnow().strftime("%Y-%m-%d")),
        }

    def apply_tool_action(self, call: dict[str, Any]) -> dict[str, Any]:
        name = call.get("name")
        arguments = call.get("arguments", {})
        if name == "save_user_note":
            save_note(arguments.get("person", "default"), arguments.get("info", ""))
            return {"status": "ok", "action": "save_user_note"}
        if name == "add_routine":
            from database import add_routine
            add_routine(arguments.get("time", "00:00"), arguments.get("task", ""))
            return {"status": "ok", "action": "add_routine"}
        if name == "remove_routine":
            from database import remove_routine
            remove_routine(arguments.get("time", "00:00"))
            return {"status": "ok", "action": "remove_routine"}
        if name == "change_setting":
            set_setting(arguments.get("key", ""), arguments.get("value", ""))
            return {"status": "ok", "action": "change_setting"}
        if name == "move_robot":
            return {"status": "ok", "action": "move_robot", "params": arguments}
        if name == "stop_robot":
            return {"status": "ok", "action": "stop_robot"}
        if name == "set_led":
            return {"status": "ok", "action": "set_led", "params": arguments}
        if name == "set_alarm":
            return {"status": "ok", "action": "set_alarm", "params": arguments}
        if name == "set_volume":
            return {"status": "ok", "action": "set_volume", "params": arguments}
        if name == "turn_on_light":
            return {"status": "ok", "action": "turn_on_light", "params": arguments}
        if name == "turn_off_light":
            return {"status": "ok", "action": "turn_off_light", "params": arguments}
        return {"status": "queued", "action": name, "params": arguments}
