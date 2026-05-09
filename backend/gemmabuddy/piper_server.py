import os
import shutil
import subprocess
import tempfile
from pathlib import Path

from fastapi import FastAPI, Response
import httpx
from pydantic import BaseModel


class TtsRequest(BaseModel):
    text: str
    voice: str
    format: str = "opus"


app = FastAPI(title="GemmaBuddy Piper TTS")


def voice_paths(voice: str) -> tuple[Path, Path]:
    voice_root = Path(os.environ.get("PIPER_VOICE_DIR", "/voices"))
    if voice.endswith(".onnx"):
        model_path = Path(voice)
    else:
        model_path = voice_root / f"{voice}.onnx"
    return model_path, Path(f"{model_path}.json")


def voice_url_path(voice: str) -> str:
    parts = voice.split("-")
    if len(parts) < 3:
        return f"{voice}.onnx"
    language, speaker, quality = parts[0], parts[1], parts[2]
    return f"{language[:2]}/{language}/{speaker}/{quality}/{voice}.onnx"


async def ensure_voice(voice: str) -> Path:
    model_path, config_path = voice_paths(voice)
    if model_path.exists() and config_path.exists():
        return model_path
    if voice.endswith(".onnx"):
        raise FileNotFoundError(f"Piper voice model not found: {voice}")

    model_path.parent.mkdir(parents=True, exist_ok=True)
    base_url = os.environ.get("PIPER_VOICE_BASE_URL", "https://huggingface.co/rhasspy/piper-voices/resolve/main")
    remote_model = f"{base_url}/{voice_url_path(voice)}"
    remote_config = f"{remote_model}.json"
    async with httpx.AsyncClient(timeout=120, follow_redirects=True) as client:
        for url, path in ((remote_model, model_path), (remote_config, config_path)):
            if path.exists():
                continue
            response = await client.get(url)
            response.raise_for_status()
            path.write_bytes(response.content)
    return model_path


@app.get("/healthz")
async def healthz() -> dict[str, str]:
    return {"status": "ok", "service": "gemmabuddy-tts"}


@app.post("/api/tts")
async def synthesize(request: TtsRequest) -> Response:
    piper_bin = os.environ.get("PIPER_BIN", "piper")
    if shutil.which(piper_bin) is None:
        return Response(b"GEMMABUDDY_TTS_PLACEHOLDER", media_type="audio/opus")

    with tempfile.TemporaryDirectory() as tmpdir:
        wav_path = os.path.join(tmpdir, "speech.wav")
        model_path = await ensure_voice(request.voice)
        subprocess.run(
            [piper_bin, "--model", str(model_path), "--output_file", wav_path],
            input=request.text.encode("utf-8"),
            check=True,
        )
        with open(wav_path, "rb") as wav_file:
            return Response(wav_file.read(), media_type="audio/wav")
