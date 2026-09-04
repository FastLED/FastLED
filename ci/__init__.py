"""FastLED CI tools and utilities.

Importing the ``ci`` package arranges two things every entrypoint depends on
and none of them should have to set up itself: console output is made safe on
Windows before any Unicode status text is printed, and ``SSL_CERT_FILE`` is
pointed at a CA bundle that exists before any HTTPS call (see
``ci.util.tls_trust``). Both helpers are lightweight and best-effort only, so
package import never fails because of them.
"""

_ci_initialized = False


def _ensure_init() -> None:
    """Lazily configure UTF-8 console on first call. Thread-safe via GIL."""
    global _ci_initialized  # noqa: PLW0603
    if _ci_initialized:
        return
    _ci_initialized = True
    try:
        from ci.util.console_utf8 import configure_utf8_console

        configure_utf8_console()
    except KeyboardInterrupt:
        import _thread  # noqa: PLC0415

        _thread.interrupt_main()
        raise
    except Exception:
        # Never fail import due to console configuration
        pass
    try:
        from ci.util.tls_trust import ensure_tls_trust_store

        ensure_tls_trust_store()
    except KeyboardInterrupt:
        import _thread  # noqa: PLC0415

        _thread.interrupt_main()
        raise
    except Exception:
        # Never fail import due to trust-store setup; a genuine TLS problem is
        # still reported by whoever makes the request.
        pass


_ensure_init()
