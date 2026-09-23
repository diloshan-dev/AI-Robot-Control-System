# Kaniye Companion Robot

This project scaffolds a Sinhala/English mixed companion robot system split across an ESP32 firmware layer and a Python AI server.

## Structure

- `firmware/robot_esp32` — ESP-IDF project for local safety and motion logic.
- `server` — FastAPI server for Gemini AI orchestration, command routing, and websocket control.

## Firmware behavior

- Emergency stop handling in FreeRTOS task form.
- Local motor control with queued command traffic.
- Separate safety, motion, and transport modules for clean responsibility boundaries.

## Python server behavior

- `/health` health check.
- `/command` command queue endpoint.
- `/chat` AI stub endpoint.
- `/ws/robot` websocket stream for robot telemetry and command flow.

## Run

```bash
cd server
python -m venv .venv
. .venv\Scripts\activate
pip install -r requirements.txt
python robot_server.py
```

For ESP32, open the `firmware/robot_esp32` directory in an ESP-IDF environment and build with:

```bash
idf.py build
```
