# Demo Script

1. Build and flash the firmware for `CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_ePaper_1_54_v2`.
2. Set `CONFIG_OTA_URL` to `https://<your-domain>/xiaozhi/ota/`.
3. Start the backend:

```bash
cp .env.example .env
docker compose up --build
```

4. Hold BOOT and say:

```text
¿Qué temperatura hay y apaga la luz del escritorio?
```

Expected behavior:

- GemmaBuddy receives Opus audio through WebSocket.
- Gemma 4 E4B-it returns transcript, emotion, response text, and allowed IoT tool calls.
- GemmaBuddy dispatches allowlisted tools only.
- The device receives `stt`, `llm`, `tts` events and audio frames.
- Logs show no calls to official XiaoZhi services.
