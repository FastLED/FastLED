"""BLE serial interface adapter for FastLED validation.

Implements the SerialInterface protocol using Bleak (cross-platform BLE GATT client).
This allows RpcClient to communicate over BLE using the same interface as serial.

Usage:
    iface = BleInterface(device_name="FastLED-C6")
    await iface.connect()
    await iface.write('{"jsonrpc":"2.0","method":"ping","id":1}\\n')
    async for line in iface.read_lines(timeout=10.0):
        print(line)
    await iface.close()
"""

from __future__ import annotations

import asyncio
from collections.abc import AsyncIterator

from bleak import BleakClient, BleakGATTCharacteristic, BleakScanner


# GATT UUIDs — must match FL_BLE_*_UUID in ble.h
BLE_SERVICE_UUID = "12345678-1234-1234-1234-123456789abc"
BLE_CHAR_RX_UUID = "12345678-1234-1234-1234-123456789ab0"  # host -> device (WRITE)
BLE_CHAR_TX_UUID = "12345678-1234-1234-1234-123456789ab1"  # device -> host (NOTIFY)


class BleInterface:
    """Async BLE interface implementing the SerialInterface protocol.

    Adapts Bleak BLE GATT operations to the same async interface that
    PySerialAdapter and FbuildSerialAdapter provide.
    """

    def __init__(
        self,
        device_name: str = "FastLED-C6",
        scan_timeout: float = 15.0,
    ) -> None:
        self._device_name = device_name
        self._scan_timeout = scan_timeout
        self._client: BleakClient | None = None
        self._rx_queue: asyncio.Queue[str] = asyncio.Queue()

    def _notification_handler(
        self, _sender: BleakGATTCharacteristic, data: bytearray
    ) -> None:
        """Callback invoked by Bleak when device sends a NOTIFY on TX char."""
        text = data.decode("utf-8", errors="replace").strip()
        if text:
            self._rx_queue.put_nowait(text)

    async def connect(self) -> None:
        """Scan for device by name, connect, and subscribe to TX notifications."""
        print(f"  [BLE] Scanning for '{self._device_name}'...")
        device = await BleakScanner.find_device_by_name(
            self._device_name, timeout=self._scan_timeout
        )
        if device is None:
            raise RuntimeError(
                f"BLE device '{self._device_name}' not found within {self._scan_timeout}s"
            )
        print(f"  [BLE] Found device: {device.name} ({device.address})")

        client = BleakClient(device, timeout=30.0)
        await client.connect()
        self._client = client
        print(f"  [BLE] Connected to {device.address}")

        # Subscribe to NOTIFY on TX characteristic
        await client.start_notify(BLE_CHAR_TX_UUID, self._notification_handler)
        print(f"  [BLE] Subscribed to TX notifications")

    async def close(self) -> None:
        """Disconnect from BLE device."""
        if self._client and self._client.is_connected:
            try:
                await self._client.stop_notify(BLE_CHAR_TX_UUID)
            except KeyboardInterrupt:
                import _thread

                _thread.interrupt_main()
                raise
            except Exception:
                pass
            await self._client.disconnect()
            print(f"  [BLE] Disconnected")
        self._client = None

    async def write(self, data: str) -> None:
        """Write string data to RX characteristic (host -> device)."""
        if not self._client or not self._client.is_connected:
            raise RuntimeError("BLE not connected")
        payload = data.encode("utf-8")
        await self._client.write_gatt_char(BLE_CHAR_RX_UUID, payload, response=True)

    async def read_lines(self, timeout: float) -> AsyncIterator[str]:
        """Async iterator yielding lines received via BLE NOTIFY.

        Args:
            timeout: Maximum time to wait for lines in seconds.

        Yields:
            Lines from BLE notifications (stripped of whitespace).
        """
        deadline = asyncio.get_event_loop().time() + timeout
        while True:
            remaining = deadline - asyncio.get_event_loop().time()
            if remaining <= 0:
                break
            try:
                line = await asyncio.wait_for(self._rx_queue.get(), timeout=remaining)
                yield line
            except asyncio.TimeoutError:
                break
