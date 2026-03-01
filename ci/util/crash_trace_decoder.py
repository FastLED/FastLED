"""Real-time ESP32 crash trace decoder for serial output.

Watches serial lines for ESP32 crash patterns (MEPC, Backtrace, register dump,
abort), accumulates crash dump lines, extracts addresses, and decodes them
using addr2line from the toolchain. Designed to be plugged into any serial
reading loop via process_line().

Usage:
    decoder = CrashTraceDecoder(build_dir=Path("."), environment="esp32c6")
    for line in serial_lines:
        extra = decoder.process_line(line)
        for decoded_line in extra:
            print(decoded_line)
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path
from typing import Optional

from ci.decode_esp32_backtrace import (
    extract_addresses_from_crash_log,
    find_addr2line,
    find_addr2line_from_build_info,
)


# Patterns that indicate the start of a crash dump.
_CRASH_START_PATTERNS: list[re.Pattern[str]] = [
    re.compile(r"abort\(\) was called", re.IGNORECASE),
    re.compile(r"Guru Meditation Error", re.IGNORECASE),
    re.compile(r"register dump", re.IGNORECASE),
    re.compile(r"\bMEPC\s*:", re.IGNORECASE),
    re.compile(r"Backtrace:", re.IGNORECASE),
    re.compile(r"Stack memory:", re.IGNORECASE),
    re.compile(r"Panic cause:", re.IGNORECASE),
    re.compile(r"assert failed", re.IGNORECASE),
]

# Lines that are part of a crash dump (to keep accumulating).
_CRASH_CONTINUATION_PATTERNS: list[re.Pattern[str]] = [
    re.compile(r"\bMEPC\s*:", re.IGNORECASE),
    re.compile(r"\bRA\s*:\s*0x", re.IGNORECASE),
    re.compile(r"\bSP\s*:\s*0x", re.IGNORECASE),
    re.compile(r"register dump", re.IGNORECASE),
    re.compile(r"Backtrace:", re.IGNORECASE),
    re.compile(r"Stack memory:", re.IGNORECASE),
    re.compile(r"0x[0-9a-fA-F]{8}:\s+0x[0-9a-fA-F]{8}"),  # Stack dump lines
    re.compile(r"Guru Meditation", re.IGNORECASE),
    re.compile(r"Panic cause:", re.IGNORECASE),
    re.compile(r"Core\s+\d", re.IGNORECASE),
    re.compile(r"\b[A-Z][A-Z0-9]\s*:\s*0x[0-9a-fA-F]+"),  # Register values
    re.compile(r"assert failed", re.IGNORECASE),
    re.compile(r"abort\(\)", re.IGNORECASE),
    re.compile(r"EPC\d\s*:", re.IGNORECASE),  # Xtensa exception PC
    re.compile(r"EXCVADDR\s*:", re.IGNORECASE),
]

# Maximum lines to accumulate before forcing a decode attempt.
_MAX_CRASH_LINES = 60

# After crash start, how many consecutive non-crash lines before we consider
# the dump finished.
_END_GRACE_LINES = 2


class CrashTraceDecoder:
    """Stateful decoder that watches serial output for ESP32 crash traces.

    Call ``process_line(line)`` for every serial line. It returns an empty list
    during normal operation and a list of decoded/formatted strings when a
    crash dump is complete.
    """

    def __init__(
        self,
        build_dir: Path,
        environment: str,
        *,
        use_fbuild: bool = False,
    ) -> None:
        self._build_dir = build_dir
        self._environment = environment
        self._use_fbuild = use_fbuild

        # Resolved lazily on first crash.
        self._elf_path: Optional[Path] = None
        self._addr2line: Optional[Path] = None
        self._resolved = False

        # Accumulation state.
        self._in_crash = False
        self._crash_lines: list[str] = []
        self._grace_count = 0
        self._crash_detected = False  # True once we've seen at least one crash

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    @property
    def crash_detected(self) -> bool:
        """Whether at least one crash has been detected so far."""
        return self._crash_detected

    def process_line(self, line: str) -> list[str]:
        """Feed a serial line and return any decoded output lines.

        Returns:
            Empty list during normal output. When a crash dump ends, returns
            a list of formatted strings (banner + decoded trace).
        """
        stripped = line.strip()

        if not self._in_crash:
            if self._is_crash_start(stripped):
                self._in_crash = True
                self._crash_detected = True
                self._crash_lines = [stripped]
                self._grace_count = 0
            return []

        # Already accumulating a crash.
        if self._is_crash_continuation(stripped):
            self._crash_lines.append(stripped)
            self._grace_count = 0
        elif stripped == "":
            # Blank lines are common within dumps; allow a few.
            self._grace_count += 1
        else:
            self._grace_count += 1

        if (
            self._grace_count >= _END_GRACE_LINES
            or len(self._crash_lines) >= _MAX_CRASH_LINES
        ):
            return self._finish_crash()

        return []

    def flush(self) -> list[str]:
        """Force-finish any in-progress crash accumulation."""
        if self._in_crash and self._crash_lines:
            return self._finish_crash()
        return []

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _finish_crash(self) -> list[str]:
        """Decode accumulated crash lines and reset state."""
        lines = self._decode()
        self._in_crash = False
        self._crash_lines = []
        self._grace_count = 0
        return lines

    @staticmethod
    def _is_crash_start(line: str) -> bool:
        return any(p.search(line) for p in _CRASH_START_PATTERNS)

    @staticmethod
    def _is_crash_continuation(line: str) -> bool:
        return any(p.search(line) for p in _CRASH_CONTINUATION_PATTERNS)

    # ------------------------------------------------------------------
    # ELF / addr2line resolution
    # ------------------------------------------------------------------

    def _resolve_tools(self) -> None:
        """Lazily find the ELF and addr2line paths."""
        if self._resolved:
            return
        self._resolved = True

        # --- ELF ---
        if self._use_fbuild:
            elf_candidates = [
                self._build_dir
                / ".fbuild"
                / "build"
                / self._environment
                / "firmware.elf",
            ]
        else:
            elf_candidates = [
                self._build_dir
                / ".fbuild"
                / "build"
                / self._environment
                / "firmware.elf",
                self._build_dir / ".pio" / "build" / self._environment / "firmware.elf",
            ]

        for p in elf_candidates:
            if p.exists():
                self._elf_path = p
                break

        if not self._elf_path:
            print(
                f"[CrashTraceDecoder] Warning: firmware.elf not found for '{self._environment}'",
                file=sys.stderr,
            )

        # --- addr2line ---
        # Try build_info.json first (fbuild).
        build_info_path = (
            self._build_dir
            / ".fbuild"
            / "build"
            / self._environment
            / "build_info.json"
        )
        if build_info_path.exists():
            self._addr2line = find_addr2line_from_build_info(build_info_path)

        # Fall back to PlatformIO toolchain search.
        if not self._addr2line:
            self._addr2line = find_addr2line()

        if not self._addr2line:
            print(
                "[CrashTraceDecoder] Warning: addr2line not found; crash traces will not be decoded",
                file=sys.stderr,
            )

    def _decode(self) -> list[str]:
        """Extract addresses from crash lines and decode via addr2line."""
        self._resolve_tools()

        crash_text = "\n".join(self._crash_lines)
        addresses = extract_addresses_from_crash_log(crash_text)

        if not addresses:
            return self._format_banner(
                ["(no decodable addresses found in crash output)"]
            )

        if not self._elf_path or not self._addr2line:
            detail = []
            if not self._elf_path:
                detail.append("ELF file not found")
            if not self._addr2line:
                detail.append("addr2line not found")
            return self._format_banner(
                [f"(cannot decode: {', '.join(detail)})"]
                + [f"  raw addresses: {' '.join(addresses)}"]
            )

        # Run addr2line.
        cmd = [
            str(self._addr2line),
            "-e",
            str(self._elf_path),
            "-f",  # function names
            "-C",  # demangle
        ] + addresses

        try:
            result = subprocess.run(
                cmd, capture_output=True, text=True, timeout=10, check=False
            )
        except subprocess.TimeoutExpired:
            return self._format_banner(["(addr2line timed out)"])
        except FileNotFoundError:
            return self._format_banner([f"(addr2line not found at {self._addr2line})"])

        if result.returncode != 0:
            return self._format_banner([f"(addr2line failed: {result.stderr.strip()})"])

        # Parse addr2line output (alternating function / location lines).
        decoded_lines: list[str] = []
        output_lines = result.stdout.strip().split("\n")
        for i in range(0, len(output_lines), 2):
            func = output_lines[i] if i < len(output_lines) else "??"
            loc = output_lines[i + 1] if i + 1 < len(output_lines) else "??:0"
            addr = addresses[i // 2] if i // 2 < len(addresses) else "0x????????"
            decoded_lines.append(f"{addr}: {func}")
            decoded_lines.append(f"             at {loc}")

        return self._format_banner(decoded_lines)

    @staticmethod
    def _format_banner(body_lines: list[str]) -> list[str]:
        """Wrap decoded output in a visual banner."""
        sep = "\u2550" * 51  # box-drawing double horizontal
        out: list[str] = [
            "",
            sep,
            " CRASH DETECTED \u2014 Decoded Stack Trace",
            sep,
        ]
        out.extend(body_lines)
        out.append(sep)
        out.append("")
        return out
