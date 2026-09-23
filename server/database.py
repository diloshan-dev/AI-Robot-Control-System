from __future__ import annotations

import json
import sqlite3
from contextlib import closing
from datetime import datetime
from pathlib import Path
from typing import Any

DB_PATH = Path(__file__).resolve().parent / "robot_data.db"


def _connect() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db() -> None:
    with closing(_connect()) as conn:
        conn.executescript(
            """
            CREATE TABLE IF NOT EXISTS settings (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL,
                updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            );

            CREATE TABLE IF NOT EXISTS personal_notes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                person TEXT NOT NULL,
                info TEXT NOT NULL,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            );

            CREATE TABLE IF NOT EXISTS routines (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                time TEXT NOT NULL,
                task TEXT NOT NULL,
                active INTEGER NOT NULL DEFAULT 1,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            );

            CREATE TABLE IF NOT EXISTS conversation_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                person TEXT NOT NULL DEFAULT 'default',
                speaker TEXT NOT NULL,
                message TEXT NOT NULL,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            );

            CREATE TABLE IF NOT EXISTS daily_logs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                log_date TEXT NOT NULL UNIQUE,
                summary TEXT NOT NULL,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            );
            """
        )
        conn.commit()


def set_setting(key: str, value: Any) -> None:
    with closing(_connect()) as conn:
        conn.execute(
            "INSERT INTO settings(key, value) VALUES(?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = CURRENT_TIMESTAMP",
            (key, str(value)),
        )
        conn.commit()


def get_setting(key: str, default: Any = None) -> Any:
    with closing(_connect()) as conn:
        row = conn.execute("SELECT value FROM settings WHERE key = ?", (key,)).fetchone()
    if row is None:
        return default
    return row["value"]


def save_note(person: str, info: str) -> None:
    with closing(_connect()) as conn:
        conn.execute(
            "INSERT INTO personal_notes(person, info) VALUES(?, ?)",
            (person, info),
        )
        conn.commit()


def get_notes_for_person(person: str) -> list[dict[str, str]]:
    with closing(_connect()) as conn:
        rows = conn.execute(
            "SELECT person, info, created_at FROM personal_notes WHERE person = ? ORDER BY created_at DESC",
            (person,),
        ).fetchall()
    return [dict(row) for row in rows]


def add_routine(time: str, task: str) -> None:
    with closing(_connect()) as conn:
        conn.execute(
            "INSERT INTO routines(time, task, active) VALUES(?, ?, 1)",
            (time, task),
        )
        conn.commit()


def remove_routine(time: str) -> None:
    with closing(_connect()) as conn:
        conn.execute("DELETE FROM routines WHERE time = ?", (time,))
        conn.commit()


def list_routines() -> list[dict[str, Any]]:
    with closing(_connect()) as conn:
        rows = conn.execute(
            "SELECT id, time, task, active FROM routines WHERE active = 1 ORDER BY time ASC"
        ).fetchall()
    return [dict(row) for row in rows]


def append_conversation(person: str, speaker: str, message: str) -> None:
    with closing(_connect()) as conn:
        conn.execute(
            "INSERT INTO conversation_history(person, speaker, message) VALUES(?, ?, ?)",
            (person, speaker, message),
        )
        conn.commit()


def fetch_recent_conversations(person: str, limit: int = 50) -> list[dict[str, Any]]:
    with closing(_connect()) as conn:
        rows = conn.execute(
            "SELECT person, speaker, message, created_at FROM conversation_history WHERE person = ? ORDER BY id DESC LIMIT ?",
            (person, limit),
        ).fetchall()
    return [dict(row) for row in rows][::-1]


def get_daily_log(date: str) -> str | None:
    with closing(_connect()) as conn:
        row = conn.execute("SELECT summary FROM daily_logs WHERE log_date = ?", (date,)).fetchone()
    if row is None:
        return None
    return row["summary"]


def save_daily_log(date: str, summary: str) -> None:
    with closing(_connect()) as conn:
        conn.execute(
            "INSERT INTO daily_logs(log_date, summary) VALUES(?, ?) ON CONFLICT(log_date) DO UPDATE SET summary = excluded.summary, created_at = CURRENT_TIMESTAMP",
            (date, summary),
        )
        conn.commit()


def default_settings() -> None:
    set_setting("ai_name", "Kaniye")
    set_setting("owner_name", "Diloshan")
    set_setting("language", "Sinhala/English")
    set_setting("mode", "helpful_companion")
    set_setting("personality", "warm, calm, and proactive")
    set_setting("word_limit", "150")


def build_system_context() -> str:
    today = datetime.utcnow().strftime("%Y-%m-%d %H:%M UTC")
    ai_name = get_setting("ai_name", "Kaniye")
    owner_name = get_setting("owner_name", "Owner")
    language = get_setting("language", "Sinhala/English")
    mode = get_setting("mode", "helpful_companion")
    personality = get_setting("personality", "warm and respectful")
    word_limit = get_setting("word_limit", "150")
    notes = []
    for row in personal_notes_snapshot():
        notes.append(f"{row['person']}: {row['info']}")
    routines = [f"{row['time']} -> {row['task']}" for row in list_routines()]
    return (
        f"You are {ai_name}, an AI companion robot for {owner_name}. "
        f"Preferred language: {language}. Mode: {mode}. Personality: {personality}. "
        f"Current date/time: {today}. Word limit: {word_limit} words unless asked for poems/songs or detailed explanations. "
        f"Saved personal notes: {', '.join(notes) if notes else 'none'}. "
        f"Active routines: {', '.join(routines) if routines else 'none'}."
    )


def personal_notes_snapshot() -> list[dict[str, str]]:
    with closing(_connect()) as conn:
        rows = conn.execute(
            "SELECT person, info, created_at FROM personal_notes ORDER BY created_at DESC LIMIT 100"
        ).fetchall()
    return [dict(row) for row in rows]


def conversation_summary_for_person(person: str) -> str:
    history = fetch_recent_conversations(person, 40)
    if not history:
        return ""
    return "\n".join(f"{row['speaker']}: {row['message']}" for row in history)


def upsert_daily_log(date: str, summary: str) -> None:
    save_daily_log(date, summary)


def serialize_json(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False)
