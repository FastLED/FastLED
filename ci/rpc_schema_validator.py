"""
JSON-RPC Schema Validator

Fetches JSON-RPC schema from device and validates requests/responses using Pydantic.
This catches schema mismatches early before sending commands to the device.

Usage:
    from ci.rpc_schema_validator import RpcSchemaValidator

    validator = RpcSchemaValidator(port='COM18')

    # Validate a request before sending
    try:
        validator.validate_request('runSingleTest', {'driver': 'PARLIO', 'laneSizes': [10]})
        # Send request to device...
    except ValidationError as e:
        print(f"Invalid request: {e}")
"""

import json
import time
from typing import Any, Optional, cast

import serial
from pydantic import BaseModel, ValidationError

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


class ParamInfo(BaseModel):
    """RPC parameter information"""

    name: str
    type: str


class MethodInfo(BaseModel):
    """RPC method information"""

    description: Optional[str] = ""
    returnType: str
    params: list[ParamInfo]
    tags: Optional[list[str]] = []


class RpcSchema(BaseModel):
    """Complete RPC schema"""

    jsonrpc: str
    methods: dict[str, MethodInfo]


class RpcSchemaValidator:
    """
    Validates JSON-RPC requests and responses against device schema.

    Fetches schema once from device and caches it for validation.
    """

    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 5.0):
        """
        Initialize validator and fetch schema from device.

        Args:
            port: Serial port (e.g., 'COM18' or '/dev/ttyUSB0')
            baudrate: Serial baud rate
            timeout: Timeout for schema fetch
        """
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.schema: Optional[RpcSchema] = None
        self._fetch_schema()

    def _fetch_schema(self):
        """Fetch schema from device via getSchema RPC call"""
        try:
            with serial.Serial(self.port, self.baudrate, timeout=self.timeout) as ser:
                time.sleep(2.0)  # Wait for device application to fully initialize
                ser.reset_input_buffer()

                # Send rpc.discover request (built-in RPC schema method)
                request: dict[str, Any] = {
                    "method": "rpc.discover",
                    "params": [],
                    "id": 1,
                    "jsonrpc": "2.0",
                }
                ser.write((json.dumps(request) + "\n").encode())
                ser.flush()

                # Read response (with timeout)
                start = time.time()
                lines_read = 0
                while time.time() - start < self.timeout:
                    if ser.in_waiting:
                        line = ser.readline().decode("utf-8", errors="ignore").strip()
                        lines_read += 1

                        # Debug: print first few lines if not getting REMOTE: response
                        if lines_read <= 10 and not line.startswith("REMOTE:"):
                            print(f"  [debug] Line {lines_read}: {line[:80]}")

                        if line.startswith("REMOTE:"):
                            # Parse JSON-RPC response
                            json_str = line[len("REMOTE:") :].strip()
                            response = json.loads(json_str)

                            if "result" in response:
                                self.schema = RpcSchema(**response["result"])
                                print(
                                    f"✅ Fetched schema: {len(self.schema.methods)} methods"
                                )
                                return
                            elif "error" in response:
                                raise RuntimeError(
                                    f"Schema fetch failed: {response['error']}"
                                )

                    time.sleep(0.01)

                raise TimeoutError(
                    f"Timeout waiting for schema response (read {lines_read} lines)"
                )

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            raise RuntimeError(f"Failed to fetch RPC schema: {e}")

    def validate_request(
        self, method: str, args: dict[str, Any] | list[Any] | Any
    ) -> None:
        """
        Validate RPC request parameters against schema.

        Args:
            method: Method name (e.g., 'runSingleTest')
            args: Method arguments (object or array)

        Raises:
            ValidationError: If request doesn't match schema
            KeyError: If method not found in schema
        """
        if not self.schema:
            raise RuntimeError("Schema not loaded")

        if method not in self.schema.methods:
            raise KeyError(
                f"Method '{method}' not found in schema. Available: {list(self.schema.methods.keys())}"
            )

        method_info = self.schema.methods[method]

        # Validate against expected parameters
        # Note: The RPC system unwraps single-element arrays, so we validate the object directly
        if not isinstance(args, dict):
            raise ValidationError(
                f"Expected object for {method}, got {type(args).__name__}"
            )

        # Type narrowing: after isinstance check, args is guaranteed to be dict
        args_dict = cast(dict[str, Any], args)

        # Check required parameters
        expected_params: set[str] = {p.name for p in method_info.params}
        provided_params: set[str] = set(args_dict.keys())

        # For now, just warn about mismatches (can be made stricter)
        missing: set[str] = expected_params - provided_params
        extra: set[str] = provided_params - expected_params

        errors: list[str] = []
        if missing:
            errors.append(f"Missing required parameters: {missing}")
        if extra:
            errors.append(f"Unknown parameters: {extra}")

        if errors:
            raise ValidationError(
                f"Validation errors for {method}: {'; '.join(errors)}"
            )

    def get_method_schema(self, method: str) -> MethodInfo:
        """Get schema for a specific method"""
        if not self.schema:
            raise RuntimeError("Schema not loaded")

        if method not in self.schema.methods:
            raise KeyError(f"Method '{method}' not found")

        return self.schema.methods[method]

    def list_methods(self) -> list[str]:
        """List all available RPC methods"""
        if not self.schema:
            raise RuntimeError("Schema not loaded")

        return list(self.schema.methods.keys())


def main():
    """Test schema validator"""
    import sys

    if len(sys.argv) < 2:
        print("Usage: python rpc_schema_validator.py <port>")
        sys.exit(1)

    port = sys.argv[1]

    try:
        validator = RpcSchemaValidator(port)

        print("\n📋 Available methods:")
        for method in validator.list_methods():
            info = validator.get_method_schema(method)
            params = ", ".join(f"{p.name}:{p.type}" for p in info.params)
            print(f"  - {method}({params}) -> {info.returnType}")

        print("\n✅ Schema validator ready")

        # Example validation
        print("\n🧪 Testing validation:")
        try:
            validator.validate_request(
                "runSingleTest",
                {"driver": "PARLIO", "laneSizes": [10], "iterations": 1},
            )
            print("  ✅ Valid request")
        except ValidationError as e:
            print(f"  ❌ Invalid request: {e}")

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        print("\n⚠️  Interrupted by user")
        sys.exit(130)
    except Exception as e:
        print(f"❌ Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
