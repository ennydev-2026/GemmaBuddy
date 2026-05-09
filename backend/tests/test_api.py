from fastapi.testclient import TestClient
from starlette.websockets import WebSocketDisconnect

from gemmabuddy.config import Settings, get_settings
from gemmabuddy.gemma import GemmaAdapter
from gemmabuddy.main import app
from gemmabuddy.schemas import GemmaTurn


def override_settings() -> Settings:
    return Settings(
        PUBLIC_DOMAIN="example.com",
        DEVICE_TOKEN="test-token",
        GEMMA_RUNTIME_URL="http://gemma.invalid/v1/chat/completions",
        PIPER_URL="http://piper.invalid/api/tts",
    )


app.dependency_overrides[get_settings] = override_settings


def test_healthz() -> None:
    with TestClient(app) as client:
        assert client.get("/healthz").json() == {"status": "ok", "service": "gemmabuddy"}


def test_ota_response_contains_websocket_config() -> None:
    with TestClient(app) as client:
        data = client.get("/xiaozhi/ota/").json()

    assert data["websocket"] == {
        "url": "wss://example.com/xiaozhi/ws",
        "token": "test-token",
        "version": 1,
    }
    assert data["firmware"] == {}
    assert "timestamp" in data["server_time"]


def test_websocket_rejects_invalid_token() -> None:
    with TestClient(app) as client:
        try:
            with client.websocket_connect("/xiaozhi/ws", headers={"Authorization": "Bearer bad"}):
                raise AssertionError("invalid token unexpectedly connected")
        except WebSocketDisconnect as exc:
            assert exc.code == 1008


def test_gemma_parser_valid_json() -> None:
    turn = GemmaAdapter.parse_turn(
        {
            "transcript": "enciende la luz",
            "emotion": "happy",
            "speak": "Listo.",
            "tool_calls": [{"name": "self.display.set_emotion", "arguments": {"emotion": "happy"}}],
        }
    )
    assert turn.transcript == "enciende la luz"
    assert turn.emotion == "happy"
    assert turn.tool_calls[0].name == "self.display.set_emotion"


def test_gemma_parser_invalid_json_fallback() -> None:
    turn = GemmaAdapter.parse_turn("not-json")
    assert turn.emotion == "neutral"
    assert turn.speak


def test_gemma_parser_normalizes_ollama_drift() -> None:
    turn = GemmaAdapter.parse_turn(
        {
            "transcript": "hola",
            "emotion": "friendly",
            "speak": True,
            "tool_calls": None,
        }
    )
    assert turn.emotion == "neutral"
    assert turn.speak == "hola"
    assert turn.tool_calls == []


def test_websocket_turn(monkeypatch) -> None:
    async def fake_turn(self, opus_frames):
        assert opus_frames == [b"opus-frame"]
        return GemmaTurn(
            transcript="temperatura",
            emotion="neutral",
            speak="La temperatura no está disponible todavía.",
            tool_calls=[{"name": "self.sensor.climate", "arguments": {}}],
        )

    monkeypatch.setattr(GemmaAdapter, "complete_audio_turn", fake_turn)

    with TestClient(app) as client:
        with client.websocket_connect(
            "/xiaozhi/ws",
            headers={"Authorization": "Bearer test-token", "Device-Id": "device-1"},
        ) as ws:
            ws.send_json({"type": "hello", "version": 1})
            hello = ws.receive_json()
            assert hello["type"] == "hello"

            ws.send_json({"type": "listen", "state": "start"})
            ws.send_bytes(b"opus-frame")
            ws.send_json({"type": "listen", "state": "stop"})

            assert ws.receive_json()["type"] == "stt"
            assert ws.receive_json()["type"] == "llm"
            assert ws.receive_json() == {"type": "tts", "state": "start"}
            assert ws.receive_json()["state"] == "sentence_start"
            assert ws.receive_json() == {"type": "tts", "state": "stop"}
