from typing import Any


ALLOWED_TOOLS = {
    "self.sensor.climate",
    "self.power.status",
    "self.display.set_emotion",
    "self.system.sleep",
    "self.network.reconfigure",
}


class ToolRejected(ValueError):
    pass


class ToolDispatcher:
    def __init__(self) -> None:
        self.display_emotion = "neutral"

    async def dispatch(self, name: str, arguments: dict[str, Any]) -> dict[str, Any]:
        if name not in ALLOWED_TOOLS:
            raise ToolRejected(f"Tool is not allowlisted: {name}")

        if name == "self.sensor.climate":
            return {"temperature_c": None, "humidity_pct": None, "source": "device-pending"}
        if name == "self.power.status":
            return {"battery_pct": None, "charging": None, "source": "device-pending"}
        if name == "self.display.set_emotion":
            self.display_emotion = str(arguments.get("emotion", "neutral"))
            return {"ok": True, "emotion": self.display_emotion}
        if name == "self.system.sleep":
            return {"ok": True, "requested": "sleep"}
        if name == "self.network.reconfigure":
            return {"ok": True, "requested": "network_reconfigure"}

        raise ToolRejected(f"Unhandled tool: {name}")
