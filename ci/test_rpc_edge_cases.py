#!/usr/bin/env python3
"""Test script for RPC edge cases and error handling.

This script tests various edge cases that could cause issues similar to the
void function bug (assuming response.data is always a dict).

Test Categories:
1. Different result types (null, string, number, boolean, array, dict)
2. Missing fields (no result, no id, no success)
3. Error responses
4. Malformed responses
5. Type mismatches
"""

import json
from dataclasses import dataclass
from typing import Any

from ci.rpc_client import RpcResponse
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


@dataclass
class TestCase:
    """Test case for RPC response handling."""

    name: str
    json_response: str
    expected_success: bool
    expected_data_type: type
    expected_data: Any
    should_raise: type[Exception] | None = None  # Exception type if should raise


# =============================================================================
# Test Cases
# =============================================================================

TEST_CASES = [
    # Category 1: Different result types
    TestCase(
        name="void_function_null_result",
        json_response='{"jsonrpc":"2.0","result":null,"id":1}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={},
    ),
    TestCase(
        name="string_result",
        json_response='{"jsonrpc":"2.0","result":"success","id":2}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={},  # Non-dict results converted to empty dict
    ),
    TestCase(
        name="number_result",
        json_response='{"jsonrpc":"2.0","result":42,"id":3}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={},  # Non-dict results converted to empty dict
    ),
    TestCase(
        name="boolean_true_result",
        json_response='{"jsonrpc":"2.0","result":true,"id":4}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={},  # Non-dict results converted to empty dict
    ),
    TestCase(
        name="boolean_false_result",
        json_response='{"jsonrpc":"2.0","result":false,"id":5}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={},  # Non-dict results converted to empty dict
    ),
    TestCase(
        name="array_result",
        json_response='{"jsonrpc":"2.0","result":[1,2,3],"id":6}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={},  # Non-dict results converted to empty dict
    ),
    TestCase(
        name="dict_result",
        json_response='{"jsonrpc":"2.0","result":{"data":"value"},"id":7}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={"data": "value"},
    ),
    TestCase(
        name="empty_dict_result",
        json_response='{"jsonrpc":"2.0","result":{},"id":8}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={},
    ),
    # Category 2: Success field variations
    TestCase(
        name="explicit_success_true",
        json_response='{"jsonrpc":"2.0","result":{"data":"value"},"success":true,"id":9}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={"data": "value"},
    ),
    TestCase(
        name="explicit_success_false",
        json_response='{"jsonrpc":"2.0","result":{"data":"value"},"success":false,"id":10}',
        expected_success=False,
        expected_data_type=dict,
        expected_data={"data": "value"},
    ),
    TestCase(
        name="success_in_result",
        json_response='{"jsonrpc":"2.0","result":{"success":true,"data":"value"},"id":11}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={"success": True, "data": "value"},
    ),
    TestCase(
        name="success_false_in_result",
        json_response='{"jsonrpc":"2.0","result":{"success":false,"error":"failed"},"id":12}',
        expected_success=False,
        expected_data_type=dict,
        expected_data={"success": False, "error": "failed"},
    ),
    # Category 3: Error responses
    TestCase(
        name="error_response",
        json_response='{"jsonrpc":"2.0","error":{"code":-32601,"message":"Method not found"},"id":13}',
        expected_success=True,  # No explicit success=false, so defaults to True
        expected_data_type=dict,
        expected_data={"code": -32601, "message": "Method not found"},
    ),
    TestCase(
        name="error_with_null_result",
        json_response='{"jsonrpc":"2.0","error":{"code":-32602,"message":"Invalid params"},"result":null,"id":14}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={},  # result=null takes precedence, converted to {}
    ),
    # Category 4: Missing fields
    TestCase(
        name="no_result_field",
        json_response='{"jsonrpc":"2.0","id":15}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={},  # No result field, entire data used (then converted to {})
    ),
    TestCase(
        name="no_id_field",
        json_response='{"jsonrpc":"2.0","result":{"data":"value"}}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={"data": "value"},
        should_raise=ValueError,  # Client requires ID for correlation
    ),
    # Category 5: Nested structures
    TestCase(
        name="deeply_nested_result",
        json_response='{"jsonrpc":"2.0","result":{"level1":{"level2":{"level3":"deep"}}},"id":16}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={"level1": {"level2": {"level3": "deep"}}},
    ),
    TestCase(
        name="result_with_null_values",
        json_response='{"jsonrpc":"2.0","result":{"key1":null,"key2":"value"},"id":17}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={"key1": None, "key2": "value"},
    ),
    # Category 6: Unicode and special characters
    TestCase(
        name="unicode_in_result",
        json_response='{"jsonrpc":"2.0","result":{"message":"Hello 世界 🌍"},"id":18}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={"message": "Hello 世界 🌍"},
    ),
    # Category 7: Large numbers
    TestCase(
        name="large_id_near_uint32_max",
        json_response='{"jsonrpc":"2.0","result":{"data":"value"},"id":4294967295}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={"data": "value"},
    ),
    TestCase(
        name="id_zero",
        json_response='{"jsonrpc":"2.0","result":{"data":"value"},"id":0}',
        expected_success=True,
        expected_data_type=dict,
        expected_data={"data": "value"},
    ),
]


def parse_response_like_client(json_str: str, expected_id: int) -> RpcResponse:
    """Simulate the client's response parsing logic.

    This replicates the logic from RpcClient._wait_for_response()
    to test edge cases without needing a real serial connection.
    """
    data = json.loads(json_str)

    # Check ID matching (from _wait_for_response)
    response_id = data.get("id")
    if response_id is None:
        raise ValueError("Response without ID")
    if response_id != expected_id:
        raise ValueError(f"ID mismatch: expected {expected_id}, got {response_id}")

    # Extract result/error field for JSON-RPC 2.0 responses
    # Priority: result > error > empty dict (no protocol fields)
    if "result" in data:
        response_data = data["result"]
    elif "error" in data:
        response_data = data["error"]
    else:
        # No result or error - empty response
        response_data = {}

    # Determine success: void functions return null, treat as success
    if "success" in data:
        success = data.get("success", True)
    elif isinstance(response_data, dict):
        success = response_data.get("success", True)
    else:
        # Void functions return null - treat as successful execution
        success = True

    # Ensure response_data is always a dict for consistent API
    if response_data is None or not isinstance(response_data, dict):
        response_data = {}

    return RpcResponse(
        success=success, data=response_data, raw_line=json_str, _id=response_id
    )


def run_tests() -> tuple[int, int]:
    """Run all test cases and return (passed, failed) counts."""
    passed = 0
    failed = 0

    print("=" * 80)
    print("RPC EDGE CASE TESTING")
    print("=" * 80)
    print()

    for i, test in enumerate(TEST_CASES, 1):
        print(f"Test {i}/{len(TEST_CASES)}: {test.name}")
        print(
            f"  JSON: {test.json_response[:80]}{'...' if len(test.json_response) > 80 else ''}"
        )

        try:
            # Extract expected ID from JSON (or use test index if no ID)
            test_data = json.loads(test.json_response)
            expected_id = test_data.get("id", i)

            # Parse the response
            response = parse_response_like_client(
                test.json_response, expected_id=expected_id
            )

            # Verify success field
            if response.success != test.expected_success:
                print(
                    f"  ❌ FAIL: Expected success={test.expected_success}, got {response.success}"
                )
                failed += 1
                continue

            # Verify data type
            if not isinstance(response.data, test.expected_data_type):
                print(
                    f"  ❌ FAIL: Expected data type {test.expected_data_type.__name__}, got {type(response.data).__name__}"
                )
                failed += 1
                continue

            # Verify data content
            if response.data != test.expected_data:
                print(
                    f"  ❌ FAIL: Expected data {test.expected_data}, got {response.data}"
                )
                failed += 1
                continue

            print(f"  ✅ PASS")
            passed += 1

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            if test.should_raise is not None and isinstance(e, test.should_raise):
                print(f"  ✅ PASS (expected exception: {type(e).__name__})")
                passed += 1
            else:
                print(f"  ❌ FAIL: Unexpected exception: {type(e).__name__}: {e}")
                failed += 1

    print()
    print("=" * 80)
    print(f"RESULTS: {passed} passed, {failed} failed")
    print("=" * 80)

    return passed, failed


def main() -> int:
    """Run tests and return exit code."""
    passed, failed = run_tests()
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    import sys

    sys.exit(main())
