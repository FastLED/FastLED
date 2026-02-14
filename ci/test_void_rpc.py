#!/usr/bin/env python3
"""Test script for void RPC functions with async/await.

This script tests that void functions (which return null in JSON-RPC 2.0)
are handled correctly by the async RPC client.

Usage:
    python ci/test_void_rpc.py --port COM3
"""

import argparse
import asyncio
import sys

from ci.rpc_client import RpcClient
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


async def main_async(port: str, baudrate: int, use_pyserial: bool) -> int:
    """Test void RPC functionality (async main function)."""
    print(f"🔌 Connecting to {port} at {baudrate} baud...")

    async with RpcClient(port, baudrate=baudrate, use_pyserial=use_pyserial) as client:
        print("✅ Connected")

        # Test 1: Call a void function (setPins doesn't return a value)
        print("\n📡 Test 1: Void function (setPins)")
        try:
            response = await client.send("setPins", args=[{"tx": 0, "rx": 1}])
            print(f"   Response success: {response.success}")
            print(f"   Response data: {response.data}")
            print(f"   Response ID: {response._id}")

            # Verify void function response
            assert response.success, "Void function should be marked as successful"
            assert isinstance(response.data, dict), (
                "response.data should always be a dict"
            )
            print(f"   ✅ Void function handled correctly")
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"   ❌ Error: {e}")
            return 1

        # Test 2: Call a non-void function (ping returns data)
        print("\n📡 Test 2: Non-void function (ping)")
        try:
            response = await client.send("ping")
            print(f"   Response success: {response.success}")
            print(f"   Response data: {response.data}")
            print(f"   Response ID: {response._id}")

            # Verify non-void function response
            assert response.success, "ping should be marked as successful"
            assert isinstance(response.data, dict), "response.data should be a dict"
            assert len(response.data) > 0, "ping should return data"
            print(f"   ✅ Non-void function handled correctly")
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"   ❌ Error: {e}")
            return 1

        print("\n✅ All void RPC tests passed!")
        print("\n💡 Void functions return null but are treated as successful execution")

    return 0


def main() -> int:
    """Entry point wrapper for async main."""
    parser = argparse.ArgumentParser(description="Test void RPC functions")
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
