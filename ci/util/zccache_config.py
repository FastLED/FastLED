from ci.util.global_interrupt_handler import handle_keyboard_interrupt


#!/usr/bin/env python3
"""
ZCCACHE Configuration for FastLED Builds

Provides zccache lookup and stats helpers for build/test tooling.
"""

import os
import re
import shutil
from pathlib import Path

from running_process import RunningProcess

from ci.util.timestamp_print import ts_print


def get_zccache_wrapper_path() -> str | None:
    """Get the path to zccache binary.

    Search order:
    1. Project .venv (installed via uv sync / ./install)
    2. Sibling zccache repo (../zccache/target/{release,debug}) — dev builds
    3. PATH (via shutil.which)

    If not found, prints a message suggesting the user run ./install.
    """
    suffix = ".exe" if os.name == "nt" else ""
    repo_root = Path(__file__).resolve().parent.parent.parent  # ci/util/ -> repo root

    # Check project .venv first (installed release version)
    venv_candidate = (
        repo_root
        / ".venv"
        / ("Scripts" if os.name == "nt" else "bin")
        / f"zccache{suffix}"
    )
    if venv_candidate.is_file():
        return str(venv_candidate)

    # Check sibling zccache repo (pick most recently built binary)
    sibling_zccache = repo_root.parent / "zccache"
    best: str | None = None
    best_mtime: float = 0
    for profile in ("release", "debug"):
        candidate = sibling_zccache / "target" / profile / f"zccache{suffix}"
        if candidate.is_file():
            mtime = candidate.stat().st_mtime
            if mtime > best_mtime:
                best = str(candidate)
                best_mtime = mtime
    if best is not None:
        return best

    # Check PATH
    path_result = shutil.which("zccache")
    if path_result:
        return path_result

    return None


def clear_zccache_stats() -> None:
    """Clear zccache statistics (no-op).

    zccache 1.0.3 does not support --zero-stats.
    Stats are per-daemon-uptime and reset automatically on daemon restart.
    """
    pass


def show_zccache_stats() -> None:
    """Display zccache statistics if zccache is available."""
    zccache_path = get_zccache_wrapper_path()
    if not zccache_path:
        return

    # Query per-session stats then end the session
    session_id = os.environ.get("ZCCACHE_SESSION_ID", "")
    if session_id and zccache_path:
        try:
            stats_result = RunningProcess.run(
                [zccache_path, "session-stats", session_id],
                cwd=None,
                check=False,
                timeout=10,
                capture_output=True,
                text=True,
            )
            # session-stats may print to stdout or stderr
            output = (stats_result.stderr or "").strip() or (
                stats_result.stdout or ""
            ).strip()
            if stats_result.returncode == 0 and output:
                ts_print("\nZCCACHE session stats:")
                for line in output.split("\n"):
                    ts_print(f"  {line.strip()}")
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception:
            pass  # Non-fatal
        try:
            RunningProcess.run(
                [zccache_path, "session-end", session_id],
                cwd=None,
                check=False,
                timeout=10,
                capture_output=True,
            )
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception:
            pass  # Non-fatal

    try:
        result = RunningProcess.run(
            [zccache_path, "status"],
            cwd=None,
            check=False,
            timeout=10,
            capture_output=True,
        )
        if result.returncode == 0:
            # Parse zccache status output format:
            #   Compilations:  N total (X cached, Y cold, Z non-cacheable)
            #   Hit rate:      XX.X%
            lines = result.stdout.strip().split("\n")
            key_metrics: dict[str, str] = {}

            for line in lines:
                line_stripped = line.strip()
                if not line_stripped:
                    continue

                if "Compilations:" in line:
                    # Parse: "Compilations:  N total (X cached, Y cold, Z non-cacheable)"
                    key_metrics["requests"] = line_stripped.split()[1]
                    # Extract cached count from parenthetical
                    cached_match = re.search(r"\((\d+)\s+cached", line_stripped)
                    cold_match = re.search(r"(\d+)\s+cold", line_stripped)
                    if cached_match:
                        key_metrics["hits"] = cached_match.group(1)
                    if cold_match:
                        key_metrics["misses"] = cold_match.group(1)
                elif "Hit rate:" in line:
                    # Parse: "Hit rate:      XX.X%" or "Hit rate:      n/a"
                    parts = line_stripped.split(":", 1)
                    if len(parts) > 1:
                        key_metrics["hit_rate"] = parts[1].strip()

            # Display compact summary only if there's meaningful data
            has_hits = key_metrics.get("hits") not in (None, "N/A", "n/a", "0")
            has_misses = key_metrics.get("misses") not in (None, "N/A", "n/a", "0")

            if has_hits or has_misses:
                ts_print("\nZCCACHE (this build):")
                if key_metrics:
                    ts_print(
                        f"  Requests: {key_metrics.get('requests', 'N/A')} | "
                        f"Hits: {key_metrics.get('hits', 'N/A')} | "
                        f"Misses: {key_metrics.get('misses', 'N/A')} | "
                        f"Hit Rate: {key_metrics.get('hit_rate', 'N/A')}"
                    )
                else:
                    ts_print(result.stdout)
    except RuntimeError as e:
        if "timeout" in str(e).lower():
            ts_print("Warning: zccache status timed out")
        else:
            ts_print(f"Warning: Failed to retrieve zccache stats: {e}")
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        ts_print(f"Warning: Failed to retrieve zccache stats: {e}")
