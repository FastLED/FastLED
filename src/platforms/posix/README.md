# FastLED Platform: posix

POSIX helpers for networking in host environments (non‑Windows).

## Files (quick pass)
- `socket_posix.h`: Thin passthrough typedefs and function declarations for POSIX sockets (bind/connect/send/recv, getaddrinfo, etc.). Used when `FASTLED_HAS_NETWORKING` and not on Windows.
- `socket_posix.cpp`: Implementation for the declarations above.

## Behavior and constraints
- Targets non‑Windows POSIX environments; exposes standard types (sockaddr, socklen_t, etc.) and functions without redefinition.
- Blocking multiplexing (select/poll) is intentionally omitted in this API to align with WASM constraints elsewhere; prefer non‑blocking flags.

## Supported hosts and notes

- Linux, macOS, BSD‑like environments with standard sockets are supported.
- Multiplexing (select/poll/epoll/kqueue) is intentionally not wrapped here; higher‑level async models (e.g., WASM) depend on non‑blocking flows.
- Combine with `shared/ui/json` for host‑side UI control or data export; keep socket operations non‑blocking to avoid stalling render loops.
