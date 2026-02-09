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
