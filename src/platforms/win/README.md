# FastLED Platform: win

Windows‑specific networking helpers.

## Files (quick pass)
- `socket_win.h`: Normalizes POSIX‑style socket API on Windows (Winsock2). Provides typedefs, constants, and wrappers (`socket/bind/connect/...`) plus init/teardown helpers.
- `socket_win.cpp`: Implementation.

## Behavior and constraints
- Requires Winsock initialization (e.g., `WSAStartup` via helper) and cleanup; wraps errors to POSIX‑like errno values (e.g., `EWOULDBLOCK`).
- Mirrors POSIX calls for cross‑platform code while respecting Windows differences (e.g., `closesocket` vs `close`).

## Winsock initialization sequence

Before using any socket functions on Windows:

1. Call `fl::win::socket_init()` (wrapper around `WSAStartup`).
2. Use the POSIX‑style wrappers (`socket/bind/connect/send/recv`, etc.).
3. On shutdown, call `fl::win::socket_cleanup()` (wrapper around `WSACleanup`).

Limitations vs POSIX:

- File descriptors are not interchangeable with CRT file handles; always use `closesocket()` (wrapped) rather than `close()`.
- Non‑blocking and error codes map to POSIX‑like values, but not all errno values are identical. Check for `EWOULDBLOCK`/`EINPROGRESS` via wrapper helpers.
