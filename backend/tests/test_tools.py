import pytest

from gemmabuddy.tools import ToolDispatcher, ToolRejected


@pytest.mark.asyncio
async def test_dispatch_allows_known_tools() -> None:
    dispatcher = ToolDispatcher()
    result = await dispatcher.dispatch("self.display.set_emotion", {"emotion": "happy"})
    assert result == {"ok": True, "emotion": "happy"}


@pytest.mark.asyncio
async def test_dispatch_rejects_unknown_tools() -> None:
    dispatcher = ToolDispatcher()
    with pytest.raises(ToolRejected):
        await dispatcher.dispatch("home.light.off", {})
