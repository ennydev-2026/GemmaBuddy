from gemmabuddy.audio import (
    SAMPLE_RATE,
    SAMPLES_PER_FRAME,
    decode_opus_frames_to_pcm,
    pcm_to_opus_frames,
    pcm_to_wav_bytes,
    wav_to_opus_frames,
)


def test_opus_roundtrip_produces_pcm() -> None:
    pcm = b"\x00\x00" * SAMPLES_PER_FRAME
    frames = pcm_to_opus_frames(pcm)
    decoded = decode_opus_frames_to_pcm(frames)

    assert frames
    assert len(decoded) == len(pcm)


def test_wav_to_opus_frames() -> None:
    pcm = b"\x00\x00" * SAMPLE_RATE
    wav = pcm_to_wav_bytes(pcm)
    frames = wav_to_opus_frames(wav)

    assert frames
