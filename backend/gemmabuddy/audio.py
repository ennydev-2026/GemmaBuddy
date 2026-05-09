import io
import logging
import subprocess
import wave

import opuslib

logger = logging.getLogger("gemmabuddy.audio")

SAMPLE_RATE = 16000
CHANNELS = 1
FRAME_DURATION_MS = 60
SAMPLES_PER_FRAME = SAMPLE_RATE * FRAME_DURATION_MS // 1000
SAMPLE_WIDTH_BYTES = 2


def decode_opus_frames_to_pcm(opus_frames: list[bytes]) -> bytes:
    decoder = opuslib.Decoder(SAMPLE_RATE, CHANNELS)
    chunks: list[bytes] = []
    for index, frame in enumerate(opus_frames):
        if not frame:
            continue
        try:
            chunks.append(decoder.decode(frame, SAMPLES_PER_FRAME))
        except Exception:
            logger.exception("failed to decode opus frame index=%d size=%d", index, len(frame))
    return b"".join(chunks)


def pcm_to_wav_bytes(pcm: bytes) -> bytes:
    output = io.BytesIO()
    with wave.open(output, "wb") as wav:
        wav.setnchannels(CHANNELS)
        wav.setsampwidth(SAMPLE_WIDTH_BYTES)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(pcm)
    return output.getvalue()


def opus_frames_to_wav(opus_frames: list[bytes]) -> tuple[bytes, float]:
    pcm = decode_opus_frames_to_pcm(opus_frames)
    duration = len(pcm) / (SAMPLE_RATE * CHANNELS * SAMPLE_WIDTH_BYTES)
    return pcm_to_wav_bytes(pcm), duration


def wav_to_pcm_16k_mono(wav_audio: bytes) -> bytes:
    process = subprocess.run(
        [
            "ffmpeg",
            "-hide_banner",
            "-loglevel",
            "error",
            "-i",
            "pipe:0",
            "-f",
            "s16le",
            "-acodec",
            "pcm_s16le",
            "-ac",
            "1",
            "-ar",
            str(SAMPLE_RATE),
            "pipe:1",
        ],
        input=wav_audio,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    return process.stdout


def pcm_to_opus_frames(pcm: bytes) -> list[bytes]:
    encoder = opuslib.Encoder(SAMPLE_RATE, CHANNELS, opuslib.APPLICATION_AUDIO)
    bytes_per_frame = SAMPLES_PER_FRAME * CHANNELS * SAMPLE_WIDTH_BYTES
    if len(pcm) % bytes_per_frame:
        pcm += b"\x00" * (bytes_per_frame - (len(pcm) % bytes_per_frame))

    frames: list[bytes] = []
    for offset in range(0, len(pcm), bytes_per_frame):
        chunk = pcm[offset : offset + bytes_per_frame]
        if len(chunk) == bytes_per_frame:
            frames.append(encoder.encode(chunk, SAMPLES_PER_FRAME))
    return frames


def wav_to_opus_frames(wav_audio: bytes) -> list[bytes]:
    return pcm_to_opus_frames(wav_to_pcm_16k_mono(wav_audio))
