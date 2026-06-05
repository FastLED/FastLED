#!/usr/bin/env python3
"""Clean up stale agent worktrees under .claude/worktrees/.

Sub-agents dispatched with isolation=worktree create directories under
``.claude/worktrees/agent-*`` (or other names). Stale worktrees accumulate
when ``git worktree remove --force`` fails on Windows because files inside
``.git/`` are marked read-only. This helper:

  - Lists worktrees under ``.claude/worktrees/``
  - Classifies each as safe (no uncommitted changes, no unpushed commits) or
    unsafe (would lose work)
  - Removes safe worktrees, with a Windows fallback that clears the read-only
    attribute then retries, and a final fallback to ``clud trash --cross-volume``
  - Runs ``git worktree prune`` to clear stale admin entries
  - Prints a one-line summary

See FastLED issue #2610.
"""

from __future__ import annotations

import argparse
import os
import shutil
import stat
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from types import TracebackType
from typing import Any, Callable

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


PROJECT_ROOT = Path(__file__).resolve().parent.parent
WORKTREES_DIR = PROJECT_ROOT / ".claude" / "worktrees"


@dataclass
class WorktreeStatus:
    path: Path
    safe: bool
    reason: str  # human-readable explanation of classification


@dataclass
class RemovalResult:
    path: Path
    removed: bool
    method: str  # "git", "git+chmod", "clud-trash", or "skipped"
    error: str  # empty on success


def run_git(args: list[str], cwd: Path | None) -> subprocess.CompletedProcess[str]:
    """Run a git command and return the completed process (no raise)."""
    return subprocess.run(
        ["git", *args],
        cwd=str(cwd) if cwd else None,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )


def directory_size_bytes(path: Path) -> int:
    """Return total size of files under ``path`` in bytes (best-effort)."""
    total = 0
    try:
        for root, _dirs, files in os.walk(path, followlinks=False):
            for name in files:
                fp = Path(root) / name
                try:
                    total += fp.stat().st_size
                except KeyboardInterrupt as ki:
                    handle_keyboard_interrupt(ki)
                    raise
                except OSError:
                    # Permission denied, broken symlink, race -- ignore.
                    continue
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except OSError:
        pass
    return total


def format_bytes(n: int) -> str:
    units = ["B", "KB", "MB", "GB", "TB"]
    size = float(n)
    for unit in units:
        if size < 1024 or unit == units[-1]:
            return f"{size:.1f} {unit}" if unit != "B" else f"{int(size)} {unit}"
        size /= 1024
    return f"{n} B"


def classify_worktree(wt: Path) -> WorktreeStatus:
    """Decide whether ``wt`` is safe to delete.

    A worktree is safe iff:
      - It is a real git working tree (has a ``.git`` file/dir)
      - ``git status --porcelain`` is empty (no uncommitted changes)
      - ``git log @{upstream}.. --oneline`` is empty (no unpushed commits),
        OR there is no upstream and the branch has no commits beyond
        ``origin/master``.

    A worktree is treated as safe (no work to lose) if it's an orphan
    directory with no ``.git`` link — those are leftover empty dirs from
    failed prior cleanups.
    """
    if not (wt / ".git").exists():
        return WorktreeStatus(path=wt, safe=True, reason="orphan directory (no .git)")

    status = run_git(["status", "--porcelain"], cwd=wt)
    if status.returncode != 0:
        return WorktreeStatus(
            path=wt,
            safe=False,
            reason=f"git status failed: {status.stderr.strip() or status.stdout.strip()}",
        )
    if status.stdout.strip():
        return WorktreeStatus(path=wt, safe=False, reason="uncommitted changes")

    # Check unpushed commits. Try upstream first.
    upstream = run_git(["log", "@{upstream}..", "--oneline"], cwd=wt)
    if upstream.returncode == 0:
        if upstream.stdout.strip():
            return WorktreeStatus(
                path=wt, safe=False, reason="unpushed commits vs upstream"
            )
        return WorktreeStatus(path=wt, safe=True, reason="clean, in sync with upstream")

    # No upstream — compare against origin/master.
    fallback = run_git(["log", "origin/master..", "--oneline"], cwd=wt)
    if fallback.returncode != 0:
        return WorktreeStatus(
            path=wt,
            safe=False,
            reason="no upstream and cannot compare to origin/master",
        )
    if fallback.stdout.strip():
        return WorktreeStatus(
            path=wt,
            safe=False,
            reason="no upstream and has commits beyond origin/master",
        )
    return WorktreeStatus(
        path=wt, safe=True, reason="clean, no commits beyond origin/master"
    )


def clear_readonly(path: Path) -> None:
    """Clear the read-only bit on every file under ``path``.

    Equivalent to ``chmod -R u+w`` on POSIX, and clears the Windows
    ReadOnly attribute by adding write permission to every file's mode.
    """
    for root, dirs, files in os.walk(path, followlinks=False):
        for name in dirs + files:
            fp = Path(root) / name
            try:
                current = fp.lstat().st_mode
                os.chmod(fp, current | stat.S_IWUSR)
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise
            except OSError:
                continue


def remove_worktree(wt: Path) -> RemovalResult:
    """Remove ``wt`` using the three-step Windows-safe strategy."""
    # Step 1: plain git worktree remove --force
    first = run_git(["worktree", "remove", "--force", str(wt)], cwd=PROJECT_ROOT)
    if first.returncode == 0 and not wt.exists():
        return RemovalResult(path=wt, removed=True, method="git", error="")

    # Step 2: clear read-only attrs, retry
    if wt.exists():
        clear_readonly(wt)
    second = run_git(["worktree", "remove", "--force", str(wt)], cwd=PROJECT_ROOT)
    if second.returncode == 0 and not wt.exists():
        return RemovalResult(path=wt, removed=True, method="git+chmod", error="")

    # If git removed the admin entry but the directory remains, fall through
    # to manual deletion / clud trash.
    rmtree_error = ""
    if wt.exists():
        try:
            shutil.rmtree(wt, ignore_errors=False, onerror=_rmtree_onerror)
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except OSError as exc:
            rmtree_error = str(exc)
        if not wt.exists():
            run_git(["worktree", "prune"], cwd=PROJECT_ROOT)
            return RemovalResult(path=wt, removed=True, method="git+rmtree", error="")

    # Step 3: clud trash as last-resort quarantine
    clud = shutil.which("clud")
    if clud is None:
        err = rmtree_error or (second.stderr.strip() or first.stderr.strip())
        return RemovalResult(
            path=wt,
            removed=False,
            method="failed",
            error=f"git remove failed and clud not on PATH: {err}",
        )
    trash = subprocess.run(
        [clud, "trash", "--cross-volume", str(wt)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if trash.returncode == 0:
        run_git(["worktree", "prune"], cwd=PROJECT_ROOT)
        return RemovalResult(path=wt, removed=True, method="clud-trash", error="")

    return RemovalResult(
        path=wt,
        removed=False,
        method="failed",
        error=f"clud trash failed: {trash.stderr.strip() or trash.stdout.strip()}",
    )


def _rmtree_onerror(
    func: Callable[..., Any],
    path: str,
    exc_info: tuple[type[BaseException], BaseException, TracebackType],  # noqa: DCT002
) -> None:
    """shutil.rmtree onerror callback: clear read-only and retry once.

    The 3-tuple signature is Python's documented `shutil.rmtree(onerror=...)`
    contract — replacing with a dataclass would break that interface, so the
    DCT002 complexity warning is suppressed here.
    """
    try:
        os.chmod(path, stat.S_IWUSR | stat.S_IRUSR)
        func(path)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except OSError:
        # Let the outer rmtree re-raise on subsequent attempts.
        pass


def list_worktree_paths() -> list[Path]:
    """Return sorted list of immediate child directories under WORKTREES_DIR."""
    if not WORKTREES_DIR.exists():
        return []
    paths: list[Path] = []
    for p in WORKTREES_DIR.iterdir():
        if p.is_dir() and not p.is_symlink():
            paths.append(p)
    return sorted(paths)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Clean up stale agent worktrees under .claude/worktrees/",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="List worktrees and their safety status without removing anything.",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Remove every worktree, including unsafe ones (dangerous).",
    )
    args = parser.parse_args(argv)

    paths = list_worktree_paths()
    if not paths:
        print(f"No worktrees found under {WORKTREES_DIR}")
        return 0

    classifications: list[WorktreeStatus] = []
    for p in paths:
        classifications.append(classify_worktree(p))
    removed = 0
    skipped = 0
    failed = 0
    reclaimed = 0

    for cls in classifications:
        size = directory_size_bytes(cls.path)
        target = cls.safe or args.all
        prefix = "[dry-run] " if args.dry_run else ""
        action = "REMOVE" if target else "SKIP"
        print(
            f"{prefix}{action:6} {cls.path.name:40} "
            f"({format_bytes(size):>10})  {cls.reason}"
        )

        if not target:
            skipped += 1
            continue
        if args.dry_run:
            continue

        result = remove_worktree(cls.path)
        if result.removed:
            removed += 1
            reclaimed += size
            print(f"         removed via {result.method}")
        else:
            failed += 1
            print(f"         FAILED: {result.error}", file=sys.stderr)

    # Always prune admin entries at the end (cheap and harmless).
    if not args.dry_run:
        run_git(["worktree", "prune"], cwd=PROJECT_ROOT)

    summary = (
        f"\n{removed} removed, {skipped} skipped (unsafe)"
        f", {format_bytes(reclaimed)} reclaimed"
    )
    if failed:
        summary += f", {failed} failed"
    if args.dry_run:
        summary = "\n[dry-run] " + summary.lstrip()
    print(summary)

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        print("interrupted", file=sys.stderr)
        sys.exit(130)
