from functools import lru_cache

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    public_domain: str = Field(default="gemmabuddy.local", alias="PUBLIC_DOMAIN")
    device_token: str = Field(default="change-me", alias="DEVICE_TOKEN")
    gemma_model: str = Field(default="google/gemma-4-E4B-it", alias="GEMMA_MODEL")
    gemma_runtime_url: str = Field(
        default="http://gemma-runtime:8000/v1/chat/completions",
        alias="GEMMA_RUNTIME_URL",
    )
    piper_url: str = Field(default="http://tts-piper:5000/api/tts", alias="PIPER_URL")
    piper_voice: str = Field(default="es_ES-sharvard-medium", alias="PIPER_VOICE")
    timezone_offset: int = Field(default=-180, alias="TIMEZONE_OFFSET")

    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    @property
    def websocket_url(self) -> str:
        return f"wss://{self.public_domain}/xiaozhi/ws"


@lru_cache
def get_settings() -> Settings:
    return Settings()
