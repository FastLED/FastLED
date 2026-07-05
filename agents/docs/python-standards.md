# Python Code Standards

## Type Annotations
Use modern PEP 585 and PEP 604 type hints (Python 3.11+ native syntax):
- Use `list[T]`, `dict[K, V]`, `set[T]` instead of `List[T]`, `Dict[K, V]`, `Set[T]`
- Use `X | Y` for unions instead of `Union[X, Y]`
- Use `T | None` instead of `Optional[T]`
- No need for `from __future__ import annotations` (works natively in Python 3.11+)

## Structured Return Types
Prefer `@dataclass` over tuples and dicts for function return types:
- Correct: Return a `@dataclass` with named fields (e.g., `CompileResult(success=True, error_output="")`)
- Avoid: Returning `tuple[bool, str]` or `dict[str, Any]` -- unnamed fields are error-prone and hard to read
- Named fields are self-documenting, support IDE autocomplete, and make refactoring safer
- Exception: Single-value returns or well-established conventions (e.g., `divmod`) don't need a dataclass

## Exception Handling - KeyboardInterrupt
- **CRITICAL**: NEVER silently catch or suppress `KeyboardInterrupt` exceptions
- When catching broad `Exception`, ALWAYS explicitly re-raise `KeyboardInterrupt` first:
  ```python
  try:
      operation()
  except KeyboardInterrupt:
      raise  # MANDATORY: Always re-raise KeyboardInterrupt
  except Exception as e:
      logger.error(f"Operation failed: {e}")
  ```
- **Thread-aware KeyboardInterrupt handling**: When handling KeyboardInterrupt in worker threads, use the global interrupt handler:
  ```python
  from ci.util.global_interrupt_handler import notify_main_thread

  try:
      # Worker thread operation
      operation()
  except KeyboardInterrupt:
      print("KeyboardInterrupt: Cancelling operation")
      notify_main_thread()  # Propagate interrupt to main thread
      raise
  ```
- `notify_main_thread()` calls `_thread.interrupt_main()` to ensure the main thread receives the interrupt signal
- This is essential for multi-threaded applications to properly coordinate shutdown across all threads
- KeyboardInterrupt (Ctrl+C) is a user signal to stop execution - suppressing it creates unresponsive processes
- This applies to ALL exception handlers, including hook handlers, cleanup code, and background tasks
- Ruff's **BLE001** (blind-except) rule can detect this but is NOT active by default
- Consider enabling BLE001 with `--select BLE001` for automatic detection

## Device serial - No raw pyserial
- **NEVER open a device serial port with `pyserial` (`import serial; serial.Serial(...)`)** in autoresearch, bench runners, or any CI code. Route through fbuild's native (Rust) serial monitor.
- Use `ci.util.serial_interface.create_serial_interface(port)` (returns the fbuild-backed `FbuildSerialAdapter` by default) and `ci.rpc_client.RpcClient` on top of it — the same path `ci/autoresearch/{ble,decode,driver_sweep}.py` and the coroutine tests already use. You get correct framing, mandatory JSON-RPC id correlation, retries, crash-trace decoding, and reconnect for free.
- **Correct:**
  ```python
  from ci.rpc_client import RpcClient
  from ci.util.serial_interface import create_serial_interface
  iface = create_serial_interface(port)
  client = RpcClient(port, serial_interface=iface)
  await client.connect()
  resp = await client.send("echo", args=[42])
  ```
- **Wrong:**
  ```python
  import serial
  s = serial.Serial(port, 115200, timeout=1)   # raw pyserial — banned
  ```
- **Why:** on Windows, hand-rolled pyserial request/response loops drop replies (~one per session) — `in_waiting` under-reports, a pre-write `reset_input_buffer()` races the reply, byte-at-a-time reads straddle the port timeout. Silicon-diagnosed 2026-07-04 on the LPC845 SPI/UART benches: the firmware answered every RPC; pyserial lost them. The only sanctioned pyserial touch is fbuild's own `PySerialAdapter` fallback inside `ci/util/serial_interface.py` (selected only via an explicit `use_pyserial=True`). Full rationale: `agents/docs/hardware-autoresearch.md` → "Device serial: fbuild's Rust monitor ONLY".

## Process Execution - No bare subprocess
- **NEVER use `subprocess.run()` or `subprocess.Popen()` directly** — use `RunningProcess.run()` instead
- `RunningProcess` (from the `running_process` package) wraps subprocess with proper timeout handling, streaming, and interrupt propagation
- `subprocess` is problematic: missing real-time output streaming, inconsistent timeout behavior, and no integration with the project's interrupt handling
- **Correct:**
  ```python
  from running_process import RunningProcess
  result = RunningProcess.run(["git", "status"], check=False, timeout=10, capture_output=True, text=True)
  print(result.stdout)
  ```
- **Wrong:**
  ```python
  import subprocess
  result = subprocess.run(["git", "status"], capture_output=True, text=True)
  ```
