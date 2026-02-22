#!/usr/bin/env python3
"""Test script for async RPC JSON with request ID correlation.

This script demonstrates the async/await RPC client with automatic ID management.

Usage:
    python ci/test_async_rpc.py --port COM3
"""

import argparse
import asyncio
import sys

from ci.rpc_client import RpcClient
from ci.util.serial_interface import create_serial_interface


async def main_async(port: str, baudrate: int, use_pyserial: bool) -> int:
    """Test async RPC functionality (async main function)."""
    print(f"🔌 Connecting to {port} at {baudrate} baud...")

    serial_iface = create_serial_interface(
        port, baud_rate=baudrate, use_pyserial=use_pyserial
    )
    async with RpcClient(
        port, baudrate=baudrate, serial_interface=serial_iface
    ) as client:
        print("✅ Connected")

        # Test 1: Single request with ID
        print("\n📡 Test 1: Single request with ID correlation")
        response = await client.send("ping")
        print(f"   Request ID: {client._next_id - 1}")  # Previous ID used
        print(f"   Response ID: {response._id}")
        print(f"   Response data: {response.data}")
        assert response._id is not None, "Response should have an ID"

        # Test 2: Multiple sequential requests with unique IDs
        print("\n📡 Test 2: Multiple sequential requests with unique IDs")
        for i in range(3):
            expected_id = client._next_id
            response = await client.send("ping")
            print(
                f"   Request #{i + 1} - Expected ID: {expected_id}, Response ID: {response._id}"
            )
            assert response._id == expected_id, (
                f"Response ID {response._id} should match request ID {expected_id}"
            )

        # Test 3: Verify ID wrapping at uint32 boundary
        print("\n📡 Test 3: Verify ID counter wrapping (uint32 overflow)")
        client._next_id = 0xFFFFFFFE  # Near uint32 max (4294967294)
        response1 = await client.send("ping")
        print(
            f"   Request at 0xFFFFFFFE - Response ID: {response1._id} (should be 4294967294)"
        )
        response2 = await client.send("ping")
        print(
            f"   Request at 0xFFFFFFFF - Response ID: {response2._id} (should be 4294967295)"
        )
        response3 = await client.send("ping")
        print(
            f"   Request at 0x00000000 - Response ID: {response3._id} (should wrap to 0)"
        )
        assert response3._id == 0, f"ID should wrap to 0, got {response3._id}"

        print("\n✅ All async RPC ID correlation tests passed!")
        print(
            "\n💡 ID management is internal - public API only uses response.data, response.success"
        )

    return 0


def main() -> int:
    """Entry point wrapper for async main."""
    parser = argparse.ArgumentParser(description="Test async RPC with ID correlation")
    parser.add_argument(
        "--port", required=True, help="Serial port (e.g., COM3, /dev/ttyUSB0)"
    )
    parser.add_argument("--baudrate", type=int, default=115200, help="Baud rate")
    parser.add_argument(
        "--use-pyserial", action="store_true", help="Use pyserial instead of fbuild"
    )
    args = parser.parse_args()

    return asyncio.run(main_async(args.port, args.baudrate, args.use_pyserial))


if __name__ == "__main__":
    sys.exit(main())
