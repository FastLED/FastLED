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
from typing import Any

from ci.rpc_client import RpcClient, RpcError, RpcTimeoutError
from ci.util.serial_interface import create_serial_interface


# Returned by RpcBench.call() when the device does not have the method
# bound (e.g. a build without an optional feature's RPC).
METHOD_NOT_FOUND = object()


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
        self, method: str, args: list[Any] | None = None, timeout: float | None = None
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
            msg = str(e).lower()
            if "not found" in msg or "unknown method" in msg or "method" in msg:
                return METHOD_NOT_FOUND
            return None
        # Non-empty dict result — hand it straight back.
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
