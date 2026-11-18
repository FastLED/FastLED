"""Normalized serial interface for fbuild and pyserial backends.

Provides a common async interface for pyserial and fbuild serial monitors.
Supported fbuild versions use the native async monitor; older versions retain
the thread-executor compatibility path.
"""

from __future__ import annotations

import _thread
import asyncio
import threading
import time
import warnings
from collections import deque
from collections.abc import AsyncIterator
from concurrent.futures import ThreadPoolExecutor
from contextlib import suppress
from typing import Protocol, runtime_checkable

import serial as pyserial

from ci.util.fbuild_runner import ensure_fbuild_daemon
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


def _pyserial_dtr_reset(port: str) -> bool:
    """Reset an ESP32 device using the esptool ClassicReset DTR/RTS sequence.

    Sequence: DTR=false,RTS=true (hold EN low) → DTR=true,RTS=false (release)
    → DTR=false (final state). This matches esptool's --before default-reset.
    """
    try:
        s = pyserial.Serial(port, 115200, timeout=0.1)
        try:
            s.dtr = False
            s.rts = True
            time.sleep(0.1)
            s.dtr = True
            s.rts = False
            time.sleep(0.05)
            s.dtr = False
        finally:
            s.close()
        return True
    except (OSError, pyserial.SerialException):
        return False


@runtime_checkable
class SerialInterface(Protocol):
    """Async serial interface protocol.

    All serial communication in the RPC layer goes through this interface,
    normalizing access between pyserial and fbuild backends.
    """

    async def connect(self) -> None:
        """Open serial connection."""
        ...

    async def close(self) -> None:
        """Close serial connection."""
        ...

    async def write(self, data: str) -> None:
        """Write string data to serial port."""
        ...

    async def read_lines(self, timeout: float) -> AsyncIterator[str]:
        """Async iterator yielding lines from serial port.

        Args:
            timeout: Maximum time to wait for lines in seconds.

        Yields:
            Lines from serial output (without newlines).
        """
        ...
        # Make this a valid async generator for protocol purposes
        if False:  # pragma: no cover
            yield ""

    async def reset_device(self, board: str | None) -> bool:
        """Reset the device via DTR/RTS toggling.

        Args:
            board: Board identifier for platform-specific reset sequence.
                   Pass None for generic DTR toggle.

        Returns:
            True if reset succeeded, False otherwise.
        """
        ...


class PySerialAdapter:
    """Adapter wrapping pyserial_monitor.SerialMonitor as SerialInterface."""

    def __init__(
        self,
        port: str,
        baud_rate: int = 115200,
        auto_reconnect: bool = True,
        verbose: bool = False,
    ) -> None:
        from ci.util.pyserial_monitor import SerialMonitor

        self._monitor = SerialMonitor(
            port=port,
            baud_rate=baud_rate,
            auto_reconnect=auto_reconnect,
            verbose=verbose,
        )

    async def connect(self) -> None:
        self._monitor.__enter__()

    async def close(self) -> None:
        self._monitor.__exit__(None, None, None)

    async def write(self, data: str) -> None:
        self._monitor.write(data)

    async def read_lines(self, timeout: float) -> AsyncIterator[str]:
        for line in self._monitor.read_lines(timeout=timeout):
            yield line

    async def reset_device(self, board: str | None) -> bool:
        """Reset device via direct pyserial DTR/RTS toggle."""
        return _pyserial_dtr_reset(self._monitor.port)


class FbuildSerialAdapter:
    """Adapter wrapping fbuild SerialMonitor as SerialInterface.

    fbuild's SerialMonitor uses run_until_complete() internally, which crashes
    when called from an already-running async event loop. This adapter offloads
    all sync calls to a ThreadPoolExecutor to avoid the conflict.
    """

    _warned: bool = False

    def __init__(
        self,
        port: str,
        baud_rate: int = 115200,
        auto_reconnect: bool = True,
        verbose: bool = False,
    ) -> None:
        from fbuild.api import SerialMonitor

        self._serial_monitor_type = SerialMonitor
        self._monitor_args = {
            "port": port,
            "baud_rate": baud_rate,
            "auto_reconnect": auto_reconnect,
            "verbose": verbose,
        }
        self._monitor = self._new_monitor()
        self._executor = ThreadPoolExecutor(max_workers=1)

    def _new_monitor(self):  # type: ignore[no-untyped-def]
        return self._serial_monitor_type(**self._monitor_args)

    @staticmethod
    def _is_daemon_connection_error(exc: Exception) -> bool:
        message = str(exc).lower()
        return "daemon websocket" in message or (
            "daemon" in message
            and (
                "connection refused" in message
                or "actively refused" in message
                or "forcibly closed" in message
                or "lost connection" in message
            )
        )

    def _connect_with_daemon_recovery(self):  # type: ignore[no-untyped-def]
        """Open the monitor, restarting a lost post-deploy daemon once."""
        try:
            return self._monitor.__enter__()
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception as first_error:
            if not self._is_daemon_connection_error(first_error):
                raise

            # A completed fbuild ESP32 deploy can outlive the daemon process
            # that launched its esptool child. Re-establish the daemon and
            # rebuild SerialMonitor so it does not retain the dead WebSocket.
            try:
                ensure_fbuild_daemon()
                self._monitor = self._new_monitor()
                return self._monitor.__enter__()
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise
            except Exception as recovery_error:
                raise recovery_error from first_error

    def _warn_once(self) -> None:
        if not FbuildSerialAdapter._warned:
            FbuildSerialAdapter._warned = True
            warnings.warn(
                "Converting sync serial monitor (fbuild) to async via thread "
                "executor. This will be removed when fbuild provides native "
                "async support.",
                stacklevel=3,
            )

    async def _run_in_thread(self, func, *args):  # type: ignore[no-untyped-def]
        self._warn_once()
        loop = asyncio.get_running_loop()
        return await loop.run_in_executor(self._executor, func, *args)

    async def connect(self) -> None:
        await self._run_in_thread(self._connect_with_daemon_recovery)

    async def close(self) -> None:
        await self._run_in_thread(self._monitor.__exit__, None, None, None)
        # NOTE: do NOT shut down self._executor here. The autoresearch
        # recovery path (ci/autoresearch/gpio.py) calls close() then reuses
        # the same adapter on a new RpcClient. Shutting the executor down
        # makes the next _run_in_thread fail with "cannot schedule new
        # futures after shutdown" and breaks DTR-reset retries. See
        # FastLED/fbuild#592.

    async def write(self, data: str) -> None:
        await self._run_in_thread(self._monitor.write, data)

    async def read_lines(self, timeout: float) -> AsyncIterator[str]:
        loop = asyncio.get_running_loop()
        queue: asyncio.Queue[str | None] = asyncio.Queue()
        # `stop_event` lets the async-generator consumer signal the
        # producer thread to exit promptly when the consumer breaks /
        # returns / is aclose()d. Without this, the producer kept
        # running for the FULL `timeout` window after the consumer
        # exited, draining serial bytes into a dead queue -- so the
        # NEXT call to `read_lines()` (queued behind the OLD producer
        # in the single-worker `_executor`) found the response line
        # already consumed. Symptom: every 2nd RPC in a session timed
        # out even though the device responded in milliseconds. See
        # FastLED #3219 goal session 2026-06-22.
        stop_event = threading.Event()

        def _producer() -> None:
            # fbuild's native read_lines() returns the first batch of
            # lines then exits (it does NOT keep reading until timeout).
            # We loop over multiple read_lines() calls to cover the full
            # timeout, but bail out promptly when stop_event is set so
            # the executor's single worker is free for the next call.
            deadline = time.monotonic() + timeout
            try:
                while not stop_event.is_set():
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        break
                    # Cap each inner read so we re-check stop_event
                    # regularly -- otherwise a quiet serial line lets
                    # read_lines(timeout=remaining) block for minutes
                    # ignoring the cancel signal.
                    #
                    # 0.05, not the 0.25 this used to be. A request/reply
                    # costs two reads here: the first returns the ACK
                    # promptly, the second waits for the result and blocks
                    # the full cap when the device has not answered yet. So
                    # the cap is paid once per RPC, on top of the round trip
                    # rather than overlapping it. Measured on an RP2350W over
                    # the fbuild monitor, 20 `ping` calls per point:
                    #
                    #   cap 0.005 -> 110.5 ms    cap 0.10 -> 119.9 ms
                    #   cap 0.02  -> 110.1 ms    cap 0.25 -> 330.7 ms
                    #   cap 0.05  -> 110.2 ms
                    #
                    # ~110 ms is the real round trip; the old cap was adding
                    # ~220 ms of pure waiting to every call, which is 3x. The
                    # curve is flat below 0.05 so there is nothing to gain
                    # going smaller, and 0.05 still re-checks stop_event 20
                    # times a second rather than 4 -- strictly more
                    # responsive to cancellation than before.
                    inner = min(remaining, 0.05)
                    for line in self._monitor.read_lines(timeout=inner):
                        loop.call_soon_threadsafe(queue.put_nowait, line)
                        if stop_event.is_set():
                            break
            except KeyboardInterrupt:
                _thread.interrupt_main()
                raise
            except Exception as exc:
                warnings.warn(
                    f"FbuildSerialAdapter read_lines error: {exc}",
                    stacklevel=2,
                )
            finally:
                loop.call_soon_threadsafe(queue.put_nowait, None)

        # Start producer in thread
        self._executor.submit(_producer)

        try:
            while True:
                item = await queue.get()
                if item is None:
                    break
                yield item
        finally:
            # Consumer exited (normal break, return, GeneratorExit from
            # aclose()). Tell the producer to drop its leftover
            # deadline and finish ASAP so the next read_lines() call
            # isn't queued behind a zombie producer in our single-
            # worker executor.
            stop_event.set()

    async def reset_device(self, board: str | None) -> bool:
        """Reset device via fbuild daemon's reset endpoint.

        Uses wait_for_output=True to block until the device produces
        serial output (rebooted and running), avoiding fixed sleep delays.
        """
        if not hasattr(self._monitor, "reset_device"):
            return _pyserial_dtr_reset(self._monitor.port)
        try:
            result = await self._run_in_thread(
                self._monitor.reset_device, board, True, 5.0
            )
            return bool(result)
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception:
            return False


# fbuild's daemon caps a WebSocket serial attach's `open_port` at 3 s and
# counts its board-family lookup against that cap, so on a loaded host a
# healthy port can miss it. The daemon then sends an error frame that
# fbuild's Python client reports only as "session closed"
# (FastLED/fbuild#2083, FastLED#5271). A failed attach leaves no session, so
# trying again is safe; on the LPC845-BRK the next attach succeeded each time.
# The retry is best effort: fbuild 2.5.51's client also reports a definitive
# rejection (e.g. another workflow's exclusive lease) as "session closed", so
# that rejection is repeated too, costing at most ~2 s before it propagates.
# Bounded to 3 attempts so a really dead port still fails within seconds.
# Remove once fbuild ships the fix and preserves the daemon's reason.
ATTACH_ATTEMPTS = 3
ATTACH_RETRY_PAUSE_SECONDS = 1.0


def is_transient_attach_error(exc: Exception) -> bool:
    """True for the attach failure FastLED/fbuild#2083 produces.

    A client-side handshake timeout mentions the daemon WebSocket and already
    goes through the lost-daemon recovery, so it is not counted here.
    """
    return "session closed" in str(exc).lower()


class NativeFbuildSerialAdapter:
    """Use fbuild's supported async monitor without a thread or read cap."""

    def __init__(
        self,
        port: str,
        baud_rate: int = 115200,
        auto_reconnect: bool = True,
        verbose: bool = False,
    ) -> None:
        from fbuild.api import AsyncSerialMonitor

        self._monitor_type = AsyncSerialMonitor
        self._monitor_args = {
            "port": port,
            "baud_rate": baud_rate,
            "auto_reconnect": auto_reconnect,
            "verbose": verbose,
        }
        self._monitor = self._new_monitor()
        self._pending_lines: deque[str] = deque()

    def _new_monitor(self):  # type: ignore[no-untyped-def]
        return self._monitor_type(**self._monitor_args)

    async def connect(self) -> None:
        self._pending_lines.clear()
        # One handler around the whole attach, daemon recovery included, so a
        # KeyboardInterrupt anywhere in it is handled exactly once.
        try:
            await self._attach()
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise

    async def _attach(self) -> None:
        # Every `__aenter__` counts against ATTACH_ATTEMPTS, the daemon
        # recovery attempt included.
        daemon_error: Exception | None = None
        for attempt in range(1, ATTACH_ATTEMPTS + 1):
            try:  # noqa: KBI001 - connect() handles KeyboardInterrupt once for the whole attach
                await self._monitor.__aenter__()
                return
            except Exception as error:
                last = attempt == ATTACH_ATTEMPTS
                if (
                    not last
                    and daemon_error is None
                    and FbuildSerialAdapter._is_daemon_connection_error(error)
                ):
                    # A lost post-deploy daemon: restart it once.
                    daemon_error = error
                    await asyncio.to_thread(ensure_fbuild_daemon)
                    self._monitor = self._new_monitor()
                    continue
                if last or not is_transient_attach_error(error):
                    if daemon_error is not None:
                        raise error from daemon_error
                    raise
                # The failed monitor holds no daemon session, but release
                # whatever client-side state it may hold before replacing it.
                with suppress(Exception):
                    await self._monitor.__aexit__(None, None, None)
                port = self._monitor_args["port"]
                print(
                    f"[serial] attach to {port} failed ({error}); retrying "
                    f"({attempt}/{ATTACH_ATTEMPTS - 1}) in "
                    f"{ATTACH_RETRY_PAUSE_SECONDS:g}s -- FastLED/fbuild#2083",
                    flush=True,
                )
                await asyncio.sleep(ATTACH_RETRY_PAUSE_SECONDS)
                self._monitor = self._new_monitor()

    async def close(self) -> None:
        try:
            await self._monitor.__aexit__(None, None, None)
        finally:
            self._pending_lines.clear()

    async def write(self, data: str) -> None:
        await self._monitor.write(data)

    async def read_lines(self, timeout: float) -> AsyncIterator[str]:
        # Native read_lines() returns only the first available batch, so keep
        # reading until the caller's timeout like FbuildSerialAdapter does:
        # an RPC reply often arrives in a later batch than boot or log lines.
        # Keep undelivered lines if the consumer closes the generator early;
        # cancellation leaves the native queue untouched.
        deadline = time.monotonic() + timeout
        while True:
            while self._pending_lines:
                yield self._pending_lines.popleft()
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return
            self._pending_lines.extend(
                await self._monitor.read_lines(timeout=remaining)
            )

    async def reset_device(self, board: str | None) -> bool:
        try:
            return bool(
                await self._monitor.reset_device(
                    board=board, wait_for_output=True, timeout=5.0
                )
            )
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception:
            return False


def create_serial_interface(
    port: str,
    baud_rate: int = 115200,
    use_pyserial: bool = False,
    auto_reconnect: bool = True,
    verbose: bool = False,
) -> SerialInterface:
    """Factory function to create the appropriate serial interface.

    Args:
        port: Serial port path (e.g., "/dev/ttyUSB0", "COM3")
        baud_rate: Serial baud rate (default: 115200)
        use_pyserial: If True, use pyserial directly instead of fbuild
        auto_reconnect: Enable auto-reconnect (passed to underlying monitor)
        verbose: Enable verbose debug output

    Returns:
        SerialInterface implementation (pyserial, native fbuild async, or
        the older sync fbuild compatibility adapter).
    """
    if use_pyserial:
        return PySerialAdapter(
            port=port,
            baud_rate=baud_rate,
            auto_reconnect=auto_reconnect,
            verbose=verbose,
        )
    try:
        from fbuild.api import AsyncSerialMonitor
    except ImportError:
        AsyncSerialMonitor = None

    # Older fbuild wheels expose an experimental async class whose read_lines
    # uses timeout_secs and whose write returns bool. Probe the supported API
    # surface so version skew keeps the proven sync adapter path.
    if AsyncSerialMonitor is not None and all(
        hasattr(AsyncSerialMonitor, name)
        for name in ("__aenter__", "__aexit__", "in_waiting", "reset_input_buffer")
    ):
        return NativeFbuildSerialAdapter(
            port=port,
            baud_rate=baud_rate,
            auto_reconnect=auto_reconnect,
            verbose=verbose,
        )
    return FbuildSerialAdapter(
        port=port,
        baud_rate=baud_rate,
        auto_reconnect=auto_reconnect,
        verbose=verbose,
    )
