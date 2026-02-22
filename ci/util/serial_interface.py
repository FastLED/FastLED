"""Normalized serial interface for fbuild and pyserial backends.

Provides a common async interface that both pyserial and fbuild serial monitors
can be accessed through. The fbuild adapter offloads sync calls to a thread
executor when running inside an async event loop.
"""

from __future__ import annotations

import _thread
import asyncio
import warnings
from collections.abc import AsyncIterator
from concurrent.futures import ThreadPoolExecutor
from typing import Protocol, runtime_checkable


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

        self._monitor = SerialMonitor(
            port=port,
            baud_rate=baud_rate,
            auto_reconnect=auto_reconnect,
            verbose=verbose,
        )
        self._executor = ThreadPoolExecutor(max_workers=1)

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
        await self._run_in_thread(self._monitor.__enter__)

    async def close(self) -> None:
        await self._run_in_thread(self._monitor.__exit__, None, None, None)
        self._executor.shutdown(wait=False)

    async def write(self, data: str) -> None:
        await self._run_in_thread(self._monitor.write, data)

    async def read_lines(self, timeout: float) -> AsyncIterator[str]:
        loop = asyncio.get_running_loop()
        queue: asyncio.Queue[str | None] = asyncio.Queue()

        def _producer() -> None:
            try:
                for line in self._monitor.read_lines(timeout=timeout):
                    loop.call_soon_threadsafe(queue.put_nowait, line)
            except KeyboardInterrupt:
                _thread.interrupt_main()
                raise
            except Exception:
                pass
            finally:
                loop.call_soon_threadsafe(queue.put_nowait, None)

        # Start producer in thread
        self._executor.submit(_producer)

        while True:
            item = await queue.get()
            if item is None:
                break
            yield item


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
        SerialInterface implementation (PySerialAdapter or FbuildSerialAdapter)
    """
    if use_pyserial:
        return PySerialAdapter(
            port=port,
            baud_rate=baud_rate,
            auto_reconnect=auto_reconnect,
            verbose=verbose,
        )
    else:
        return FbuildSerialAdapter(
            port=port,
            baud_rate=baud_rate,
            auto_reconnect=auto_reconnect,
            verbose=verbose,
        )
