import time
import uuid
from typing import Annotated

from fastapi import Depends, FastAPI, Header, WebSocket, WebSocketDisconnect, status

from .config import Settings, get_settings
from .gemma import GemmaAdapter
from .schemas import ConversationContext, GemmaTurn
from .tools import ToolDispatcher, ToolRejected
from .tts import TtsAdapter

app = FastAPI(title="GemmaBuddy", version="0.1.0")


def ota_payload(settings: Settings) -> dict:
    return {
        "server_time": {
            "timestamp": int(time.time() * 1000),
            "timezone_offset": settings.timezone_offset,
        },
        "websocket": {
            "url": settings.websocket_url,
            "token": settings.device_token,
            "version": 1,
        },
        "firmware": {},
    }


@app.get("/healthz")
async def healthz() -> dict[str, str]:
    return {"status": "ok", "service": "gemmabuddy"}


@app.get("/xiaozhi/ota/")
async def get_ota(settings: Settings = Depends(get_settings)) -> dict:
    return ota_payload(settings)


@app.post("/xiaozhi/ota/")
async def post_ota(settings: Settings = Depends(get_settings)) -> dict:
    return ota_payload(settings)


def token_from_authorization(authorization: str | None) -> str | None:
    if not authorization:
        return None
    if authorization.lower().startswith("bearer "):
        return authorization[7:].strip()
    return authorization.strip()


async def send_server_hello(websocket: WebSocket, session_id: str) -> None:
    await websocket.send_json(
        {
            "type": "hello",
            "transport": "websocket",
            "session_id": session_id,
            "audio_params": {
                "format": "opus",
                "sample_rate": 16000,
                "channels": 1,
                "frame_duration": 60,
            },
        }
    )


async def send_turn(websocket: WebSocket, turn: GemmaTurn, tts: TtsAdapter) -> None:
    if turn.transcript:
        await websocket.send_json({"type": "stt", "text": turn.transcript})

    await websocket.send_json(
        {
            "type": "llm",
            "text": turn.speak,
            "emotion": turn.emotion,
        }
    )
    await websocket.send_json({"type": "tts", "state": "start"})
    await websocket.send_json({"type": "tts", "state": "sentence_start", "text": turn.speak})
    for frame in await tts.synthesize_opus_frames(turn.speak):
        await websocket.send_bytes(frame)
    await websocket.send_json({"type": "tts", "state": "stop"})


@app.websocket("/xiaozhi/ws")
async def xiaozhi_ws(
    websocket: WebSocket,
    authorization: Annotated[str | None, Header()] = None,
    settings: Settings = Depends(get_settings),
) -> None:
    if token_from_authorization(authorization) != settings.device_token:
        await websocket.close(code=status.WS_1008_POLICY_VIOLATION)
        return

    await websocket.accept()
    session_id = str(uuid.uuid4())
    context = ConversationContext(
        session_id=session_id,
        device_id=websocket.headers.get("device-id"),
        client_id=websocket.headers.get("client-id"),
    )
    gemma = GemmaAdapter(settings)
    tts = TtsAdapter(settings)
    tools = ToolDispatcher()

    try:
        while True:
            message = await websocket.receive()
            if message.get("type") == "websocket.disconnect":
                return
            if "bytes" in message and message["bytes"] is not None:
                context.audio_frames.append(message["bytes"])
                continue

            data = message.get("text")
            if not data:
                continue

            import json

            event = json.loads(data)
            event_type = event.get("type")
            if event_type == "hello":
                await send_server_hello(websocket, session_id)
            elif event_type == "listen" and event.get("state") == "start":
                context.audio_frames.clear()
            elif event_type == "listen" and event.get("state") == "stop":
                turn = await gemma.complete_audio_turn(context.audio_frames)
                for call in turn.tool_calls:
                    try:
                        await tools.dispatch(call.name, call.arguments)
                    except ToolRejected:
                        continue
                await send_turn(websocket, turn, tts)
            elif event_type == "mcp":
                await websocket.send_json({"type": "mcp", "payload": {"status": "received"}})
    except WebSocketDisconnect:
        return
