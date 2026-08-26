"""Unit tests for device-to-device network AutoResearch flows."""

from __future__ import annotations

import asyncio
import base64
from typing import Any
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from ci.autoresearch.net import run_net_peer_autoresearch
from ci.autoresearch.ota import run_ota_peer_autoresearch


def _response(data: dict[str, Any]) -> MagicMock:
    response = MagicMock()
    response.data = data
    return response


def test_ota_peer_reports_missing_firmware(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A missing fbuild artifact fails cleanly before opening either board."""
    result = asyncio.run(
        run_ota_peer_autoresearch(
            upload_port="COM18",
            peer_upload_port="COM9",
            serial_iface=None,
            firmware_path=None,
        )
    )

    assert result == 1
    assert "RP2350W firmware is missing: None" in capsys.readouterr().out


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
    client_test_timeouts: list[float] = []

    async def primary_send(method: str, *_args: Any, **_kwargs: Any) -> MagicMock:
        primary_methods.append(method)
        if method == "runNetClientTest":
            client_test_timeouts.append(_kwargs["timeout"])
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
        if method == "runNetClientTest":
            client_test_timeouts.append(_kwargs["timeout"])
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
    assert len(client_test_timeouts) == 20
    assert all(call_timeout > 20.0 for call_timeout in client_test_timeouts)
    assert "ping" in peer_methods
    primary.close.assert_awaited_once()
    peer.close.assert_awaited_once()


def test_ota_peer_stages_artifact_without_a_host_wifi_manager(tmp_path) -> None:
    """OTA peer validation uses only RPC/fbuild channels before device WiFi."""
    artifact = tmp_path / "firmware.bin"
    artifact.write_bytes(b"firmware")
    primary_before = MagicMock()
    primary_after = MagicMock()
    peer = MagicMock()
    for client in (primary_before, primary_after, peer):
        client.connect = AsyncMock()
        client.close = AsyncMock()

    async def primary_before_send(
        method: str, *_args: Any, **_kwargs: Any
    ) -> MagicMock:
        responses = {
            "status": {"platform": "Raspberry Pi Pico 2 W (RP2350)"},
            "wifiConnect": {"success": True},
            "wifiStatus": {"connected": True},
            "applyOtaArtifact": {"success": True},
            "stopNet": {"success": True},
        }
        return _response(responses[method])

    async def primary_after_send(method: str, *_args: Any, **_kwargs: Any) -> MagicMock:
        return _response({"success": True})

    async def peer_send(method: str, *_args: Any, **_kwargs: Any) -> MagicMock:
        responses = {
            "status": {"platform": "ESP32-C6 (RISC-V)"},
            "beginOtaArtifact": {"success": True},
            "writeOtaArtifact": {"success": True},
            "finishOtaArtifact": {
                "success": True,
                "sha256": "c3bf47ea1f4a4a605470313cacb3a44f4a461f68c6faeab07e737610cb5ac835",
            },
            "startNetServer": {
                "success": True,
                "ssid": "FastLED-AutoResearch",
                "password": "fastled123",
            },
            "startOtaArtifactServer": {
                "success": True,
                "ip": "192.168.4.1",
                "port": 8081,
            },
            "otaArtifactStatus": {"success": True, "servedRequests": 1},
            "stopNet": {"success": True},
        }
        return _response(responses[method])

    primary_before.send = AsyncMock(side_effect=primary_before_send)
    primary_after.send = AsyncMock(side_effect=primary_after_send)
    peer.send = AsyncMock(side_effect=peer_send)
    with (
        patch(
            "ci.autoresearch.ota.RpcClient",
            side_effect=[primary_before, peer, primary_after],
        ),
        patch("ci.util.serial_interface.create_serial_interface"),
        patch("ci.autoresearch.ota.asyncio.sleep", new_callable=AsyncMock),
    ):
        result = asyncio.run(
            run_ota_peer_autoresearch(
                upload_port="COM17",
                peer_upload_port="COM9",
                serial_iface=MagicMock(),
                firmware_path=artifact,
                timeout=60.0,
            )
        )

    assert result == 0
    assert peer.send.await_args_list[1].args[0] == "beginOtaArtifact"
    write_calls = [
        call for call in peer.send.await_args_list if call.args[0] == "writeOtaArtifact"
    ]
    assert len(write_calls) == 1
    encoded = write_calls[0].args[1]
    assert isinstance(encoded, str), "RpcClient performs the one-parameter wrapping"
    assert base64.b64decode(encoded) == b"firmware"
