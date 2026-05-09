# GemmaBuddy Architecture

GemmaBuddy keeps the ESP-IDF firmware protocol compatible with XiaoZhi devices while replacing the official service dependency with a self-hosted backend.

```mermaid
flowchart LR
    Device["Waveshare ESP32-S3 e-Paper 1.54 V2"] -->|"GET/POST /xiaozhi/ota/"| API["FastAPI GemmaBuddy"]
    Device -->|"WSS /xiaozhi/ws JSON + Opus"| API
    API --> Gemma["Gemma 4 E4B-it runtime"]
    API --> Piper["Piper TTS"]
    API --> Tools["GemmaBuddy IoT tools"]
```

The `/xiaozhi/*` path is retained only as a firmware compatibility surface. Product copy, docs, runtime logs, and deployment names are GemmaBuddy.

No official XiaoZhi service is required.
