import httpx
import logging

from .config import Settings

logger = logging.getLogger("gemmabuddy.tts")


class TtsAdapter:
    def __init__(self, settings: Settings, client: httpx.AsyncClient | None = None) -> None:
        self.settings = settings
        self.client = client or httpx.AsyncClient(timeout=30)

    async def synthesize_opus_frames(self, text: str) -> list[bytes]:
        try:
            response = await self.client.post(
                self.settings.piper_url,
                json={"text": text, "voice": self.settings.piper_voice, "format": "opus"},
            )
            response.raise_for_status()
            audio = response.content
            if audio.startswith(b"GEMMABUDDY_TTS_PLACEHOLDER") or audio.startswith(b"RIFF"):
                logger.warning("tts backend did not return Opus; sending text-only turn")
                return []
        except Exception:
            logger.exception("tts backend unavailable; sending text-only turn")
            return []

        return [audio[index : index + 960] for index in range(0, len(audio), 960)] or [b""]
