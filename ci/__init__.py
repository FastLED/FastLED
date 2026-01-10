"""FastLED CI tools and utilities

Module-level initialization to ensure consistent runtime behavior across tools.
"""

from ci.util.global_interrupt_handler import notify_main_thread

# Configure UTF-8 console output on Windows globally for CI tools
try:
    from ci.util.console_utf8 import configure_utf8_console

    configure_utf8_console()
except KeyboardInterrupt:
    notify_main_thread()
    raise
except Exception:
    # Never fail import due to console configuration
    pass
