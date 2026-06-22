"""Audible+visual alert when an automation step is blocked on a human.

Usage from any Python (CI script, autoresearch, agent helper):

    from ci.util.blocker_alert import blocker_alert
    blocker_alert(
        "Teensy 4 wedged in flexioRxBenchmark — power-cycle COM20 and re-run.",
        details=["device last seen 8s ago", "RX channel begin() returned false"],
    )

What it does:
- Prints the message to stderr in bold red (ANSI 1;31m), so it stands out in
  any modern terminal that respects ANSI.
- Plays a 3-tone "URGENT" pattern via winsound.Beep — DELIBERATELY chosen so
  it does NOT sound like a system notification (no MessageBeep). Pattern is
  a falling-rising-falling chirp at 1175 Hz / 440 Hz / 1175 Hz with 180 ms
  per tone, repeated twice. Memorable, hard to confuse with Outlook /
  Slack / Windows toasts.
- On non-Windows, falls back to four terminal-bell characters (`\\a`).
- Never raises — alert plumbing must not break the caller's error path.
"""

from __future__ import annotations

import sys
import time
from typing import Sequence

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


_ANSI_BOLD_RED = "\x1b[1;31m"
_ANSI_RED = "\x1b[31m"
_ANSI_RESET = "\x1b[0m"


def _play_beep_pattern() -> None:
    """Distinctive 3-tone-then-repeat chirp via winsound.Beep on Windows.

    Tones picked to be unmistakably synthetic and outside the band of
    common system sounds (Windows uses ~800 Hz monotones). Falling-then-
    rising shape sounds like an alarm, not a notification.
    """
    if sys.platform != "win32":
        # POSIX terminal bell × 4, spaced so it's audibly multiple beeps.
        for _ in range(4):
            sys.stderr.write("\a")
            sys.stderr.flush()
            time.sleep(0.15)
        return

    try:
        # `winsound` is Windows-only; typeshed marks `Beep` as conditionally
        # available, so we bind it locally and let the broad except below
        # catch AttributeError on the unlikely path where `Beep` is absent.
        from winsound import Beep  # type: ignore[import-not-found]

        # First pass.
        Beep(1175, 180)  # D6
        Beep(440, 180)  # A4
        Beep(1175, 180)  # D6
        time.sleep(0.12)
        # Second pass — confirms it's a deliberate alert, not a stray ding.
        Beep(1175, 180)
        Beep(440, 180)
        Beep(1175, 180)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        # winsound.Beep can raise on locked-down systems / no audio device.
        # Fall back to terminal bell so the user still gets *something*.
        for _ in range(4):
            sys.stderr.write("\a")
            sys.stderr.flush()
            time.sleep(0.15)


def blocker_alert(
    message: str,
    *,
    details: Sequence[str] | None = None,
    silent: bool = False,
) -> None:
    """Print a bold-red blocker banner and play the distinctive beep pattern.

    Args:
        message: One-line headline of what the automation is stuck on.
        details: Optional supplementary lines (each printed plain-red below
            the banner). Use for "what to check" hints.
        silent: Skip the audible beep (kept for unit tests).
    """
    try:
        sys.stderr.write("\n")
        sys.stderr.write(
            f"{_ANSI_BOLD_RED}==================== BLOCKED — HUMAN ACTION REQUIRED ===================={_ANSI_RESET}\n"
        )
        sys.stderr.write(f"{_ANSI_BOLD_RED}{message}{_ANSI_RESET}\n")
        if details:
            for line in details:
                sys.stderr.write(f"{_ANSI_RED}  - {line}{_ANSI_RESET}\n")
        sys.stderr.write(
            f"{_ANSI_BOLD_RED}=========================================================================={_ANSI_RESET}\n"
        )
        sys.stderr.flush()
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        # Output failure must not mask the underlying blocker.
        pass

    if not silent:
        try:
            _play_beep_pattern()
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception:
            pass


if __name__ == "__main__":
    # Smoke test: invoke as `uv run python ci/util/blocker_alert.py` to hear
    # the alert pattern and see the red banner without breaking anything.
    blocker_alert(
        "SMOKE TEST — this is what a blocker alert looks/sounds like.",
        details=[
            "if you saw this banner in red AND heard the falling-rising chirp twice, the alert works",
            "if either was missing, check terminal ANSI support / audio device",
        ],
    )
