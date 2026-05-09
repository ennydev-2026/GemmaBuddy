import base64
import json
from typing import Any

import httpx
from pydantic import ValidationError

from .config import Settings
from .schemas import GemmaTurn


SYSTEM_PROMPT = """You are GemmaBuddy, a concise voice IoT assistant running on a Waveshare ESP32-S3 e-paper device.
Return only valid JSON with keys: transcript, emotion, speak, tool_calls.
Allowed emotions: happy, neutral, sleepy, surprised, sad.
Allowed tools: self.sensor.climate, self.power.status, self.display.set_emotion, self.system.sleep, self.network.reconfigure.
Keep spoken responses short and natural."""


class GemmaAdapter:
    def __init__(self, settings: Settings, client: httpx.AsyncClient | None = None) -> None:
        self.settings = settings
        self.client = client or httpx.AsyncClient(timeout=60)

    async def complete_audio_turn(self, opus_frames: list[bytes]) -> GemmaTurn:
        if self.settings.gemma_provider.lower() == "ollama":
            return await self.complete_with_ollama(opus_frames)
        return await self.complete_with_openai_compatible(opus_frames)

    async def complete_with_openai_compatible(self, opus_frames: list[bytes]) -> GemmaTurn:
        audio_b64 = base64.b64encode(b"".join(opus_frames)).decode("ascii")
        payload: dict[str, Any] = {
            "model": self.settings.gemma_model,
            "messages": [
                {"role": "system", "content": SYSTEM_PROMPT},
                {
                    "role": "user",
                    "content": [
                        {"type": "text", "text": "Process this Opus voice turn and choose IoT tools if needed."},
                        {
                            "type": "input_audio",
                            "input_audio": {"data": audio_b64, "format": "opus"},
                        },
                    ],
                },
            ],
            "temperature": 0.2,
            "response_format": {"type": "json_object"},
        }

        try:
            response = await self.client.post(self.settings.gemma_runtime_url, json=payload)
            response.raise_for_status()
            raw = response.json()["choices"][0]["message"]["content"]
            return self.parse_turn(raw)
        except Exception:
            return GemmaTurn(
                transcript="",
                emotion="neutral",
                speak="No pude procesar eso todavía.",
                tool_calls=[],
            )

    async def complete_with_ollama(self, opus_frames: list[bytes]) -> GemmaTurn:
        payload: dict[str, Any] = {
            "model": self.settings.gemma_model,
            "stream": False,
            "format": "json",
            "messages": [
                {"role": "system", "content": SYSTEM_PROMPT},
                {
                    "role": "user",
                    "content": (
                        "You received a push-to-talk voice turn from the GemmaBuddy device. "
                        "The local Ollama model cannot decode Opus audio directly in this backend path yet, "
                        f"so reason from this metadata: {len(opus_frames)} Opus frame(s) received. "
                        "Return a short useful Spanish response and no unsafe tool calls."
                    ),
                },
            ],
        }

        try:
            response = await self.client.post(self.settings.gemma_runtime_url, json=payload)
            response.raise_for_status()
            raw = response.json().get("message", {}).get("content", "")
            return self.parse_turn(raw)
        except Exception:
            return GemmaTurn(
                transcript="",
                emotion="neutral",
                speak="Ollama local no respondió todavía.",
                tool_calls=[],
            )

    @staticmethod
    def parse_turn(raw: str | dict[str, Any]) -> GemmaTurn:
        try:
            data = json.loads(raw) if isinstance(raw, str) else raw
            if isinstance(data, dict):
                data = dict(data)
                if data.get("emotion") not in {"happy", "neutral", "sleepy", "surprised", "sad"}:
                    data["emotion"] = "neutral"
                if not isinstance(data.get("speak"), str):
                    data["speak"] = data.get("transcript") or "Listo."
                if not isinstance(data.get("tool_calls"), list):
                    data["tool_calls"] = []
            return GemmaTurn.model_validate(data)
        except (json.JSONDecodeError, ValidationError, TypeError):
            return GemmaTurn(
                transcript="",
                emotion="neutral",
                speak="No pude procesar eso todavía.",
                tool_calls=[],
            )
