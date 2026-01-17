"""FastLED CI tools and utilities

Module-level initialization to ensure consistent runtime behavior across tools.
"""

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


# Configure UTF-8 console output on Windows globally for CI tools
try:
    from ci.util.console_utf8 import configure_utf8_console

    configure_utf8_console()
except KeyboardInterrupt:
    handle_keyboard_interrupt_properly()
    raise
except Exception:
    # Never fail import due to console configuration
    pass
