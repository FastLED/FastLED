"""Contracts for the fbuild-backed one-method RPC CLI."""

from __future__ import annotations

from typing import Any
from unittest.mock import MagicMock

import pytest

from ci.autoresearch import rpc_bench
from ci.autoresearch.rpc_bench import METHOD_NOT_FOUND, RpcBench, _parse_cli_args_json
from ci.rpc_client import RpcError


def test_parse_cli_args_accepts_rpc_containers_and_null() -> None:
    assert _parse_cli_args_json('{"nested":{"ok":true}}') == {"nested": {"ok": True}}
    assert _parse_cli_args_json("[1,2,3]") == [1, 2, 3]
    assert _parse_cli_args_json("null") is None


def test_parse_cli_args_rejects_scalar_json() -> None:
    with pytest.raises(ValueError, match="array, object, or null"):
        _parse_cli_args_json('"scalar"')


def test_cli_rejects_method_reported_failure(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    class FailedBench:
        def __init__(self, _port: str, timeout: float) -> None:
            assert timeout == 3.0

        def __enter__(self) -> "FailedBench":
            return self

        def __exit__(self, *_exc: object) -> None:
            return None

        def call(
            self,
            method: str,
            args: dict[str, Any] | list[Any] | None,
            timeout: float,
        ) -> dict[str, Any]:
            assert method == "testRpConcurrency"
            assert args == {}
            assert timeout == 3.0
            return {"success": False, "actual": 100, "expected": 200}

    monkeypatch.setattr(rpc_bench, "RpcBench", FailedBench)

    assert rpc_bench.main(["COM17", "testRpConcurrency", "--timeout", "3"]) == 1
    output = capsys.readouterr().out
    assert "REMOTE: testRpConcurrency" in output
    assert "method reported success=false" in output
    assert "RESULT: RPC call PASS" not in output


@pytest.mark.parametrize("caller", ["call", "call_flat"])
def test_rpc_bench_uses_structured_method_not_found_code(caller: str) -> None:
    bench = object.__new__(RpcBench)
    bench._loop = MagicMock()
    bench._client = MagicMock()
    error = RpcError("Method not found", code=-32601)

    def raise_error(awaitable: Any) -> None:
        close = getattr(awaitable, "close", None)
        if close is not None:
            close()
        raise error

    bench._loop.run_until_complete.side_effect = raise_error

    assert getattr(bench, caller)("missing") is METHOD_NOT_FOUND


@pytest.mark.parametrize("caller", ["call", "call_flat"])
def test_rpc_bench_does_not_misclassify_other_structured_method_errors(
    caller: str,
) -> None:
    bench = object.__new__(RpcBench)
    bench._loop = MagicMock()
    bench._client = MagicMock()
    error = RpcError("Invalid params for method foo", code=-32602)

    def raise_error(awaitable: Any) -> None:
        close = getattr(awaitable, "close", None)
        if close is not None:
            close()
        raise error

    bench._loop.run_until_complete.side_effect = raise_error

    assert getattr(bench, caller)("foo") is None
