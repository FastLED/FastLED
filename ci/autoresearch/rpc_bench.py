"""Shared synchronous RpcClient facade for AutoResearch bench runners.

Device serial in the AutoResearch complex goes through fbuild's native
(Rust) serial monitor, NEVER raw pyserial — see
`agents/docs/hardware-autoresearch.md` -> "Device serial" and the
`ci/lint_python/pyserial_checker.py` (PYS001) ban. Raw pyserial
request/reply loops drop replies ~one-per-session on Windows; `RpcClient`
over the fbuild backend gives mandatory JSON-RPC id correlation and robust
framing.

`RpcBench` wraps the async `RpcClient` in a small synchronous facade so the
various `test_*` runners (which are written as straight-line sync scripts)
can call RPCs without each managing its own event loop.
"""

from __future__ import annotations

import json
import sys
from argparse import ArgumentParser
from typing import Any

from ci.rpc_client import RpcClient, RpcError, RpcTimeoutError
from ci.util.global_interrupt_handler import handle_keyboard_interrupt
from ci.util.serial_interface import create_serial_interface


# Returned by RpcBench.call() when the device does not have the method
# bound (e.g. a build without an optional feature's RPC).
METHOD_NOT_FOUND = object()


def _is_method_not_found(error: RpcError) -> bool:
    """Classify structured errors, with text fallback for legacy firmware."""
    if error.code == -32601:
        return True
    if error.code is not None:
        return False
    message = str(error).lower()
    return "not found" in message or "unknown method" in message or "method" in message


class RpcBench:
    """Synchronous facade over the async fbuild-backed RpcClient.

    Create once, call `.call(method, args)` per RPC, `.close()` at the end.
    The handlers in the AutoResearch harnesses return either a JSON object
    (surfaced as a dict) or a CSV string (recovered from the raw line,
    since RpcClient coerces non-dict results to {}).
    """

    def __init__(self, port: str, timeout: float = 10.0) -> None:
        import asyncio

        self._loop = asyncio.new_event_loop()
        iface = create_serial_interface(port)
        self._client = RpcClient(port, timeout=timeout, serial_interface=iface)
        self._loop.run_until_complete(
            self._client.connect(boot_wait=3.0, drain_boot=True)
        )

    def call(
        self,
        method: str,
        args: list[Any] | dict[str, Any] | None = None,
        timeout: float | None = None,
    ) -> Any:
        """Return the RPC result, None on timeout, or METHOD_NOT_FOUND.

        A dict result is returned as-is. A CSV/string result (RpcClient
        drops non-dict results to {}) is recovered from `raw_line`.
        """
        try:
            resp = self._loop.run_until_complete(
                self._client.send(method, args=args, timeout=timeout)
            )
        except RpcTimeoutError:
            return None
        except RpcError as e:
            if _is_method_not_found(e):
                return METHOD_NOT_FOUND
            return None
        # Non-empty dict result — hand it straight back.
        return self._extract_result(resp)

    def call_flat(
        self,
        method: str,
        args: list[Any] | None = None,
        timeout: float | None = None,
    ) -> Any:
        """Call RPC methods bound with positional C++ parameters."""

        async def _call_flat():
            if not self._client.is_connected:
                raise RpcError("Not connected")
            serial = self._client._serial
            if serial is None:
                raise RpcError("Not connected")
            request_id = self._client._next_id
            self._client._next_id = (request_id + 1) & 0xFFFFFFFF
            cmd = {
                "method": method,
                "params": args if args is not None else [],
                "id": request_id,
            }
            await serial.write(json.dumps(cmd, separators=(",", ":")) + "\n")
            self._client._sent_count += 1
            effective_timeout = timeout if timeout is not None else self._client.timeout
            return await self._client._wait_for_response(
                effective_timeout, expected_id=request_id
            )

        try:
            resp = self._loop.run_until_complete(_call_flat())
        except RpcTimeoutError:
            return None
        except RpcError as e:
            if _is_method_not_found(e):
                return METHOD_NOT_FOUND
            return None
        return self._extract_result(resp)

    @staticmethod
    def _extract_result(resp) -> Any:
        if isinstance(resp.data, dict) and resp.data:
            return resp.data
        # CSV/string result: recover from the raw line RpcClient preserved.
        line = resp.raw_line or ""
        for prefix in ("REMOTE: ", "RESULT: "):
            if line.startswith(prefix):
                line = line[len(prefix) :]
                break
        try:
            obj = json.loads(line)
        except (json.JSONDecodeError, ValueError):
            return resp.data
        return obj.get("result", resp.data)

    def reset_and_drain(self) -> bool:
        """Reset through the fbuild serial backend and drain boot output."""

        async def _reset() -> bool:
            serial = self._client._serial
            if serial is None:
                return False
            ok = await serial.reset_device(None)
            await self._client.drain_boot_output()
            return ok

        return bool(self._loop.run_until_complete(_reset()))

    def close(self) -> None:
        try:
            self._loop.run_until_complete(self._client.close())
        finally:
            self._loop.close()

    # Context-manager support so runners written as `with s: ...` (a
    # drop-in for `with serial.Serial(...) as s`) work unchanged.
    def __enter__(self) -> "RpcBench":
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()


def _parse_cli_args_json(value: str) -> list[Any] | dict[str, Any] | None:
    parsed = json.loads(value)
    if parsed is None or isinstance(parsed, (list, dict)):
        return parsed
    raise ValueError("RPC args must decode to an array, object, or null")


def main(argv: list[str] | None = None) -> int:
    """Call one RPC method through the repository's fbuild-backed transport."""
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("port", help="Application serial port, for example COM17")
    parser.add_argument("method", help="JSON-RPC method name")
    parser.add_argument("--args", default="{}", help="JSON array/object arguments")
    parser.add_argument("--timeout", type=float, default=10.0)
    parsed = parser.parse_args(argv)

    try:
        rpc_args = _parse_cli_args_json(parsed.args)
    except (json.JSONDecodeError, ValueError) as exc:
        parser.error(str(exc))

    try:
        with RpcBench(parsed.port, timeout=parsed.timeout) as bench:
            result = bench.call(parsed.method, args=rpc_args, timeout=parsed.timeout)
    except KeyboardInterrupt as exc:
        handle_keyboard_interrupt(exc)
        return 130
    except Exception as exc:  # noqa: BLE001 - CLI must report transport failures
        print(f"RESULT: RPC call FAIL: {exc}")
        return 1

    if result is METHOD_NOT_FOUND:
        print("RESULT: RPC call FAIL: method not found")
        return 1
    if result is None:
        print("RESULT: RPC call FAIL: timeout or transport error")
        return 1
    print(f"REMOTE: {parsed.method} -> {result!r}")
    if isinstance(result, dict) and result.get("success") is False:
        print("RESULT: RPC call FAIL: method reported success=false")
        return 1
    print("RESULT: RPC call PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
