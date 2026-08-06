"""Unit tests for device-to-device network AutoResearch flows."""

from __future__ import annotations

import asyncio
from typing import Any
from unittest.mock import AsyncMock, MagicMock, patch

from ci.autoresearch.net import run_net_peer_autoresearch


def _response(data: dict[str, Any]) -> MagicMock:
    response = MagicMock()
    response.data = data
    return response


def test_net_peer_runs_ten_device_only_reconnect_cycles() -> None:
    """The peer path uses RPC/fbuild serial only, never a host WiFi manager."""
    primary = MagicMock()
    peer = MagicMock()
    primary.connect = AsyncMock()
    primary.close = AsyncMock()
    peer.connect = AsyncMock()
    peer.close = AsyncMock()
    primary_methods: list[str] = []
    peer_methods: list[str] = []

    async def primary_send(method: str, *_args: Any, **_kwargs: Any) -> MagicMock:
        primary_methods.append(method)
        responses = {
            "status": {"platform": "Raspberry Pi Pico 2 W (RP2350)"},
            "wifiConnect": {"success": True},
            "wifiStatus": {"connected": True, "ip": "192.168.4.2"},
            "startNetServer": {"success": True, "port": 80},
            "runNetClientTest": {"success": True},
            "stopNet": {"success": True},
            "ping": {"success": True},
        }
        return _response(responses[method])

    async def peer_send(method: str, *_args: Any, **_kwargs: Any) -> MagicMock:
        peer_methods.append(method)
        responses = {
            "status": {"platform": "ESP32-C6 (RISC-V)"},
            "startNetServer": {
                "success": True,
                "ssid": "FastLED-AutoResearch",
                "password": "fastled123",
                "ip": "192.168.4.1",
                "port": 80,
            },
            "runNetClientTest": {"success": True},
            "stopNet": {"success": True},
            "ping": {"success": True},
        }
        return _response(responses[method])

    primary.send = AsyncMock(side_effect=primary_send)
    peer.send = AsyncMock(side_effect=peer_send)

    with (
        patch("ci.autoresearch.net.RpcClient", side_effect=[primary, peer]),
        patch("ci.util.serial_interface.create_serial_interface"),
    ):
        result = asyncio.run(
            run_net_peer_autoresearch(
                upload_port="COM17",
                peer_upload_port="COM9",
                serial_iface=MagicMock(),
                timeout=60.0,
            )
        )

    assert result == 0
    assert primary_methods.count("wifiConnect") == 10
    assert primary_methods.count("runNetClientTest") == 10
    assert primary_methods.count("stopNet") >= 11
    assert peer_methods.count("runNetClientTest") == 10
    assert "ping" in peer_methods
    primary.close.assert_awaited_once()
    peer.close.assert_awaited_once()
