import pytest

from gemmabuddy.audio import SAMPLES_PER_FRAME, pcm_to_wav_bytes
from gemmabuddy.config import Settings
from gemmabuddy.tts import TtsAdapter


class FakeResponse:
    def __init__(self, content: bytes, content_type: str = "audio/wav") -> None:
        self.content = content
        self.headers = {"content-type": content_type}

    def raise_for_status(self) -> None:
        return None


class FakeClient:
    def __init__(self, response: FakeResponse) -> None:
        self.response = response

    async def post(self, *args, **kwargs) -> FakeResponse:
        return self.response


@pytest.mark.asyncio
async def test_tts_converts_wav_to_opus_frames() -> None:
    wav = pcm_to_wav_bytes(b"\x00\x00" * SAMPLES_PER_FRAME)
    adapter = TtsAdapter(Settings(), client=FakeClient(FakeResponse(wav)))

    frames = await adapter.synthesize_opus_frames("Hola")

    assert frames


@pytest.mark.asyncio
async def test_tts_placeholder_returns_empty_frames() -> None:
    adapter = TtsAdapter(Settings(), client=FakeClient(FakeResponse(b"GEMMABUDDY_TTS_PLACEHOLDER", "audio/opus")))

    frames = await adapter.synthesize_opus_frames("Hola")

    assert frames == []
