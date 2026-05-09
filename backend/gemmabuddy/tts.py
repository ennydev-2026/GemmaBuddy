import httpx
import logging

from .audio import wav_to_opus_frames
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
            if audio.startswith(b"GEMMABUDDY_TTS_PLACEHOLDER"):
                logger.warning("tts backend returned placeholder; sending text-only turn")
                return []
            if not audio.startswith(b"RIFF"):
                logger.warning("tts backend returned unsupported audio content_type=%s bytes=%d", response.headers.get("content-type"), len(audio))
                return []
            frames = wav_to_opus_frames(audio)
            logger.info("tts wav_bytes=%d opus_frames=%d", len(audio), len(frames))
            return frames
        except Exception:
            logger.exception("tts backend unavailable; sending text-only turn")
            return []
