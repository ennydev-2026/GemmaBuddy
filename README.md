# GemmaBuddy

GemmaBuddy is a self-hosted voice IoT assistant for the Waveshare ESP32-S3 e-Paper 1.54 V2. It uses an ESP-IDF firmware client derived from the open-source XiaoZhi ESP32 project, but connects only to your own backend.

The firmware keeps the XiaoZhi-compatible OTA and WebSocket protocol shape for device compatibility:

- OTA config: `https://<your-domain>/xiaozhi/ota/`
- Voice WebSocket: `wss://<your-domain>/xiaozhi/ws`
- Audio transport: binary Opus frames plus JSON control messages
- Device features: e-paper face/emotions, push-to-talk, sleep/power, audio I/O, MCP tools

The backend runs Gemma 4 E4B-it for audio understanding, IoT reasoning, tool selection, and spoken response planning. Local TTS is provided by Piper.

## Repository Layout

- `main/`, `components/`, `partitions/`, `sdkconfig*`: ESP-IDF firmware.
- `main/boards/waveshare/esp32-s3-epaper-1.54/`: target board support for Waveshare ESP32-S3 e-Paper 1.54 V2.
- `backend/`: FastAPI OTA/WebSocket service, Gemma adapter, Piper TTS adapter, tool dispatcher, and tests.
- `deploy/`: reverse proxy and deployment support files.
- `docs/`: architecture and demo notes.

## Firmware Build

Install ESP-IDF, then build for ESP32-S3:

```bash
idf.py set-target esp32s3
idf.py build
```

The ESP32-S3 defaults select:

```text
CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_ePaper_1_54_v2=y
CONFIG_OTA_URL="https://gemmabuddy.local/xiaozhi/ota/"
```

For your domain, set `CONFIG_OTA_URL` with `idf.py menuconfig` under `GemmaBuddy -> Default OTA URL`, or edit `sdkconfig` for a reproducible build.

Flash and monitor:

```bash
idf.py flash monitor
```

## Backend

Copy the environment template and set your values:

```bash
cp .env.example .env
docker compose up --build
```

Required values:

- `PUBLIC_DOMAIN`: public TLS domain for the device.
- `DEVICE_TOKEN`: shared device token returned by OTA and required by WebSocket.
- `GEMMA_MODEL`: defaults to `google/gemma-4-E4B-it`.
- `GEMMA_RUNTIME_URL`: OpenAI-compatible or local runtime endpoint.
- `PIPER_VOICE`: Piper voice id/path used by the TTS service.

## Protocol

OTA returns:

```json
{
  "server_time": { "timestamp": 0, "timezone_offset": -180 },
  "websocket": {
    "url": "wss://example.com/xiaozhi/ws",
    "token": "device-token",
    "version": 1
  },
  "firmware": {}
}
```

The WebSocket accepts the firmware `hello`, `listen start`, binary Opus audio, and `listen stop`. It responds with server `hello`, `stt`, `llm` emotion, `tts` events, and binary audio frames.

## Attribution

GemmaBuddy builds on the open-source XiaoZhi ESP32 firmware and keeps a XiaoZhi-compatible protocol path for firmware interoperability. The product branding, backend, deployment, and Gemma-powered IoT assistant behavior are GemmaBuddy-specific and do not require official XiaoZhi services.
