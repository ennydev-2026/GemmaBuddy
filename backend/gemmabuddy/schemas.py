from typing import Any, Literal

from pydantic import BaseModel, Field


Emotion = Literal["happy", "neutral", "sleepy", "surprised", "sad"]


class ToolCall(BaseModel):
    name: str
    arguments: dict[str, Any] = Field(default_factory=dict)


class GemmaTurn(BaseModel):
    transcript: str = ""
    emotion: Emotion = "neutral"
    speak: str
    tool_calls: list[ToolCall] = Field(default_factory=list)


class ConversationContext(BaseModel):
    session_id: str
    device_id: str | None = None
    client_id: str | None = None
    audio_frames: list[bytes] = Field(default_factory=list)
