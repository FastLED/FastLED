from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


"""
Git Historian - Fast code search combining working tree and git history

A fast code search agent that combines working tree and git history searches to provide
context for AI assistants. Designed to complete searches in under 4 seconds by running
parallel searches and keeping results compact.
"""

import re
import shlex
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


# --- tiny helpers ------------------------------------------------------------


def _run(cmd: str, timeout: float = 3.0) -> str:
    """Run a command and return stdout (best-effort; never raises on nonzero)."""
    try:
        out = subprocess.run(
            shlex.split(cmd),
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            timeout=timeout,
            check=False,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        return out.stdout
    except subprocess.TimeoutExpired:
        return ""
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        return ""


def _have(cmd: str) -> bool:
    """Check if a command is available."""
    try:
        result = subprocess.run(
            [cmd, "--version"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=1.0,
            check=False,
        )
        return result.returncode == 0
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        return False


def _compile_alt_regex(keywords: list[str]) -> str:
    """
    Build a smart, case-insensitive alternation regex.

    Escape fixed terms but preserve regex patterns.
    If a term looks like a regex (has meta), we keep it; else escape it.
    """
    metas = set(".^$*+?{}[]|()\\")
    parts: list[str] = []
    for kw in keywords:
        if any(c in metas for c in kw):
            parts.append(kw)
        else:
            parts.append(re.escape(kw))
    # (?i) for case-insensitive (faster than adding flags in all tools)
    return "(?i)(" + "|".join(parts) + ")"


def _paths_or_repo_root(paths: list[Path]) -> list[Path]:
    """Return provided paths or default to repo root."""
    if paths:
        return paths
    # default to repo root
    root = _run("git rev-parse --show-toplevel", timeout=0.8).strip() or "."
    return [Path(root)]


# --- search implementations ---------------------------------------------------


def _search_working_tree(
    pattern: str, roots: list[Path], max_files: int = 20, max_lines_per_file: int = 3
) -> list[str]:
    """
    Return file-context chunks from the working tree.

    Format: 'FILE <path>\\nL<line>: <snippet>\\n...'
    Prefer ripgrep; fall back to 'git grep'.
    """
    chunks: list[str] = []
    roots_quoted = " ".join(shlex.quote(str(p)) for p in roots)

    if _have("rg"):
        # rg is very fast; limit results per file and total files.
        # --no-heading --line-number for parseable output
        # Use -m to cap matches per file; --max-filesize to avoid huge blobs.
        cmd = (
            f"rg --no-heading --line-number --hidden --smart-case --pcre2 "
            f"-m {max_lines_per_file} --max-filesize 1M "
            f"-e {shlex.quote(pattern)} {roots_quoted}"
        )
        out = _run(cmd, timeout=2.0)
    else:
        # fallback: search tracked files only, limit via head
        # Note: git grep doesn't have per-file cap; we cap globally.
        cmd = (
            f"git grep -n -I -e {shlex.quote(pattern)} -- "
            f"{' '.join(shlex.quote(str(p)) for p in roots)}"
        )
        out = _run(cmd, timeout=2.0)

    if not out:
        return chunks

    # Aggregate by file, capped at max_files
    hits_by_file: dict[str, list[tuple[int, str]]] = {}
    for line in out.splitlines():
        # Expect "path:line:match"
        parts = line.split(":", 2)
        if len(parts) < 3:
            continue
        fpath, lno, snippet = parts[0], parts[1], parts[2]
        try:
            ln = int(lno)
        except ValueError:
            continue
        hits_by_file.setdefault(fpath, []).append((ln, snippet))

    # rank files by #hits, then name
    ranked = sorted(hits_by_file.items(), key=lambda kv: (-len(kv[1]), kv[0]))
    for fpath, hits in ranked[:max_files]:
        # compact snippet block
        lines: list[str] = [f"FILE {fpath}"]
        for ln, snip in hits[:max_lines_per_file]:
            # trim long lines to keep context small
            if len(snip) > 220:
                snip = snip[:217] + "..."
            lines.append(f"L{ln}: {snip}")
        chunks.append("\n".join(lines))

    return chunks


def _search_last10_history(
    pattern: str, max_commits: int = 10, max_hunks_per_commit: int = 3
) -> list[str]:
    """
    Search the diffs of the last N commits for the pattern (regex).

    Returns compact diff-context chunks per matching commit.
    """
    # Use Git's -G to search diff hunks; limit to last 10 commits.
    # --unified=0 keeps hunks tiny; we'll parse filenames and +/− lines.
    cmd = (
        f"git log -n {max_commits} --date=short "
        f'--pretty="---%H %ad %s" '
        f"-G{shlex.quote(pattern)} --perl-regexp --patch --unified=0"
    )
    out = _run(cmd, timeout=2.2)
    if not out:
        return []

    chunks: list[str] = []
    cur_header = None
    cur_file = None
    hunks_this_commit = 0
    lines_buf: list[str] = []

    def _flush_commit() -> None:
        nonlocal lines_buf, hunks_this_commit, cur_header
        if cur_header and lines_buf:
            # Cap lines to keep each chunk small
            body = "\n".join(lines_buf[:18])
            chunks.append(f"COMMIT {cur_header}\n{body}")
        lines_buf = []
        hunks_this_commit = 0
        cur_header = None

    for line in out.splitlines():
        if line.startswith("---") and not line.startswith("--- a/"):
            # commit header line: ---<SHA> <date> <subject>
            # flush previous
            _flush_commit()
            cur_header = line[3:].strip()
            cur_file = None
            continue
        if line.startswith("diff --git"):
            cur_file = None
            continue
        if line.startswith("+++ b/"):
            cur_file = line[6:].strip()
            continue
        if line.startswith("@@"):
            if cur_header is None:
                continue
            if hunks_this_commit >= max_hunks_per_commit:
                # skip extra hunks to stay fast/compact
                cur_file = None
                continue
            hunks_this_commit += 1
            lines_buf.append(f"FILE {cur_file or '(unknown)'}")
            lines_buf.append(line)  # the hunk header itself
            continue
        if (
            cur_header is not None
            and cur_file is not None
            and (line.startswith("+") or line.startswith("-"))
        ):
            # Only record added/removed lines; trim length
            text = line
            if len(text) > 220:
                text = text[:217] + "..."
            lines_buf.append(text)

    _flush_commit()
    return chunks


# --- main API ----------------------------------------------------------------


def query(keywords: list[str], paths: list[Path]) -> list[str]:
    """
    Return compact 'contexts' for an AI.

    Returns:
      - working-tree file hits grouped per file
      - matching diff snippets from the last 10 commits

    Each item is a small multi-line string (~few lines) to keep tokens low.
    """
    t0 = time.time()
    if not keywords:
        return []

    pattern = _compile_alt_regex(keywords)
    roots = _paths_or_repo_root(paths)

    results: list[str] = []
    with ThreadPoolExecutor(max_workers=2) as pool:
        fut_tree = pool.submit(_search_working_tree, pattern, roots)
        fut_hist = pool.submit(_search_last10_history, pattern)

        try:
            for fut in as_completed([fut_tree, fut_hist], timeout=5.0):
                try:
                    results.extend(fut.result())
                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    raise
                except Exception:
                    # keep going even if one branch fails
                    pass
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception:
            # Timeout or other error - just use whatever results we have
            pass

    # As a final guardrail, keep total size modest (token-friendly)
    # Sort contexts: history chunks first (more "explanatory"), then files.
    history = [c for c in results if c.startswith("COMMIT ")]
    files = [c for c in results if c.startswith("FILE ")]
    compact = history[:12] + files[:20]

    # Hard cap on total contexts to avoid overbudget; prefer history.
    if len(compact) > 24:
        compact = compact[:24]

    # Ensure we finish under ~4s (lightweight safeguard; not strict)
    _ = time.time() - t0
    return compact
