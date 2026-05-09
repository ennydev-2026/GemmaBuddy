import os
import shutil
import subprocess
import tempfile

from fastapi import FastAPI, Response
from pydantic import BaseModel


class TtsRequest(BaseModel):
    text: str
    voice: str
    format: str = "opus"


app = FastAPI(title="GemmaBuddy Piper TTS")


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
        subprocess.run(
            [piper_bin, "--model", request.voice, "--output_file", wav_path],
            input=request.text.encode("utf-8"),
            check=True,
        )
        with open(wav_path, "rb") as wav_file:
            return Response(wav_file.read(), media_type="audio/wav")
