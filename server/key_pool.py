from __future__ import annotations

import threading
from typing import Any


class ApiKeyPool:
    def __init__(self, keys: list[str] | str | None, *, service: str = "api") -> None:
        raw = keys or []
        if isinstance(raw, str):
            raw = [item.strip() for item in raw.split(",") if item.strip()]
        self.service = service
        self.keys: list[str] = raw
        self.index = 0
        self.lock = threading.Lock()
        self.stats: dict[str, dict[str, int]] = {
            key: {"success": 0, "failure": 0} for key in self.keys
        }

    def __len__(self) -> int:
        return len(self.keys)

    def _mask_key(self, key: str) -> str:
        return f"{key[:4]}..." if len(key) > 8 else "***"

    def next_key(self) -> str | None:
        with self.lock:
            if not self.keys:
                return None
            key = self.keys[self.index]
            self.index = (self.index + 1) % len(self.keys)
            return key

    def current_key(self) -> str | None:
        with self.lock:
            if not self.keys:
                return None
            return self.keys[self.index]

    def mark_success(self, key: str) -> None:
        with self.lock:
            entry = self.stats.setdefault(key, {"success": 0, "failure": 0})
            entry["success"] += 1

    def mark_failure(self, key: str, reason: str) -> None:
        with self.lock:
            entry = self.stats.setdefault(key, {"success": 0, "failure": 0})
            entry["failure"] += 1

    def status(self) -> dict[str, Any]:
        with self.lock:
            return {
                "service": self.service,
                "key_count": len(self.keys),
                "keys": [
                    {
                        "index": i,
                        "masked": self._mask_key(k),
                        "success": self.stats.get(k, {}).get("success", 0),
                        "failure": self.stats.get(k, {}).get("failure", 0),
                    }
                    for i, k in enumerate(self.keys)
                ],
            }
