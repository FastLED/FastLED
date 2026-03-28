#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker and auto-fixer for FL_NOEXCEPT on special member functions.

Detects special member functions (destructors, default/copy/move constructors,
copy/move assignment operators) that are missing FL_NOEXCEPT in src/fl/.

FL_NOEXCEPT is currently a noop on all platforms for cross-platform
compatibility. Enforcing its use on special member functions ensures that
when noexcept is re-enabled, all the right places are already annotated.

Scope: src/fl/** (all .h / .hpp / .cpp / .cpp.hpp files)

Exemptions:
  - src/fl/stl/noexcept.h  (the macro definition itself)
  - Lines inside // or /* */ comments
  - Lines with suppression comment "// ok no FL_NOEXCEPT" or "// nolint"
  - Third-party code (third_party/)

Usage:
  # Lint mode (detect only):
  uv run python -m ci.lint_cpp.noexcept_special_members_checker

  # Dry-run (show what would be fixed):
  uv run python -m ci.lint_cpp.noexcept_special_members_checker --dry-run

  # Auto-fix mode:
  uv run python -m ci.lint_cpp.noexcept_special_members_checker --fix
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT

# ── Regex patterns ──────────────────────────────────────────────────────────

# Suppression comments
_SUPPRESS = re.compile(r"//\s*(?:ok\s+no\s+FL_NOEXCEPT|nolint)\b", re.IGNORECASE)

# Class/struct name extraction (captures identifier after class/struct keyword)
_CLASS_DEF = re.compile(r"\b(?:class|struct)\s+(\w+)")

# FL_NOEXCEPT or bare noexcept (either satisfies the check)
_HAS_NOEXCEPT = re.compile(r"\b(?:FL_NOEXCEPT|noexcept)\b")

# Destructor: ~Name(
_DTOR = re.compile(r"~(\w+)\s*\(")

# Assignment operator: operator=(
_OP_ASSIGN = re.compile(r"\boperator\s*=\s*\(")

# Allowed prefixes for constructor declarations (no return type)
_CTOR_QUALS = frozenset(
    {
        "",
        "explicit",
        "virtual",
        "inline",
        "constexpr",
        "explicit constexpr",
        "constexpr explicit",
        "inline explicit",
        "explicit inline",
        "inline constexpr",
        "constexpr inline",
        "virtual explicit",
        "explicit virtual",
        "inline virtual",
        "virtual inline",
    }
)

# Include line that provides FL_NOEXCEPT
_NOEXCEPT_INCLUDE = '#include "fl/stl/noexcept.h"'


# ── Helpers ─────────────────────────────────────────────────────────────────


def _find_close_paren(text: str, open_pos: int) -> int:
    """Return index of the matching ')' for '(' at *open_pos*, or -1."""
    depth = 1
    for i in range(open_pos + 1, len(text)):
        ch = text[i]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return i
    return -1


def _find_close_paren_multiline(
    lines: list[str], start_idx: int, open_col: int
) -> tuple[int, int]:
    """Find matching ')' across multiple lines.

    Returns ``(line_idx, col)`` or ``(-1, -1)`` if not found.
    """
    depth = 1
    line = lines[start_idx]
    for col in range(open_col + 1, len(line)):
        if line[col] == "(":
            depth += 1
        elif line[col] == ")":
            depth -= 1
            if depth == 0:
                return (start_idx, col)
    for j in range(1, 15):
        idx = start_idx + j
        if idx >= len(lines):
            break
        ln = lines[idx]
        for col in range(len(ln)):
            if ln[col] == "(":
                depth += 1
            elif ln[col] == ")":
                depth -= 1
                if depth == 0:
                    return (idx, col)
    return (-1, -1)


def _compute_block_comment_mask(lines: list[str]) -> list[bool]:
    """Return a list where ``mask[i]`` is True if line *i* is inside a block comment."""
    mask: list[bool] = []
    inside = False
    for line in lines:
        line_inside = inside  # state at start of this line
        i = 0
        while i < len(line):
            if inside:
                if line[i : i + 2] == "*/":
                    inside = False
                    i += 2
                    continue
            else:
                if line[i : i + 2] == "/*":
                    inside = True
                    i += 2
                    continue
                if line[i : i + 2] == "//":
                    break  # rest of line is a line-comment
            i += 1
        mask.append(line_inside)
    return mask


def _strip_line_comment(line: str) -> str:
    """Return *line* with any trailing ``//`` comment removed."""
    in_str = False
    in_char = False
    for i, ch in enumerate(line):
        if ch == '"' and not in_char and (i == 0 or line[i - 1] != "\\"):
            in_str = not in_str
        elif ch == "'" and not in_str and (i == 0 or line[i - 1] != "\\"):
            in_char = not in_char
        elif (
            not in_str
            and not in_char
            and ch == "/"
            and i + 1 < len(line)
            and line[i + 1] == "/"
        ):
            return line[:i]
    return line


# ── Detection ───────────────────────────────────────────────────────────────

# Result of classifying a line: (kind_label, open_paren_column)
MatchResult = tuple[str, int]


def classify_line(line: str, class_names: set[str]) -> MatchResult | None:
    """Detect if *line* starts a special member function declaration.

    Returns ``(kind, open_paren_col)`` or ``None``.
    """
    code = _strip_line_comment(line)
    stripped = code.strip()
    if not stripped or stripped.startswith("#"):
        return None

    # ── Destructor ──────────────────────────────────────────────────────
    m = _DTOR.search(code)
    if m:
        tilde_pos = m.start()
        # Exclude manual destructor calls: obj->~T() or obj.~T()
        prefix_text = code[:tilde_pos].rstrip()
        if prefix_text.endswith("->") or prefix_text.endswith("."):
            pass  # not a declaration
        else:
            paren = code.index("(", m.start())
            # Exclude bitwise NOT expressions: ~Type(value) — destructors
            # have empty parens
            close = _find_close_paren(code, paren)
            if close >= 0:
                inside = code[paren + 1 : close].strip()
                if inside == "" or inside == "void":
                    return ("destructor", paren)

    # ── Assignment operator ─────────────────────────────────────────────
    m = _OP_ASSIGN.search(code)
    if m:
        paren = code.index("(", m.start())
        return ("assignment operator", paren)

    # ── Constructors (copy / move / default) ────────────────────────────
    for name in class_names:
        pat = re.compile(rf"\b({re.escape(name)})\s*\(")
        for cm in pat.finditer(code):
            # Constructor prefix check: no return type before the name
            prefix = code[: cm.start()].strip()
            if prefix not in _CTOR_QUALS:
                continue

            paren = cm.end() - 1  # position of '('
            rest = code[paren + 1 :].lstrip()

            # Copy constructor: const ClassName &
            if re.match(rf"const\s+{re.escape(name)}\s*&", rest):
                return ("copy constructor", paren)

            # Move constructor: ClassName &&
            if re.match(rf"{re.escape(name)}\s*&&", rest):
                return ("move constructor", paren)

            # Default constructor: () or (void)
            if re.match(r"(?:void\s*)?\)", rest):
                return ("default constructor", paren)

    return None


def _join_multiline_signature(lines: list[str], start: int) -> str | None:
    """If line at *start* begins an unbalanced function signature, join with
    subsequent lines until parentheses balance.  Returns the joined text or
    ``None`` if the line is already balanced.
    """
    line = _strip_line_comment(lines[start])
    if "(" not in line:
        return None
    depth = 0
    for ch in line:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
    if depth <= 0:
        return None  # already balanced

    joined = line
    for j in range(1, 10):
        idx = start + j
        if idx >= len(lines):
            break
        nxt = _strip_line_comment(lines[idx]).strip()
        joined += " " + nxt
        for ch in nxt:
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
        if depth <= 0:
            return joined
    return None


def signature_has_noexcept(
    lines: list[str], start: int, open_paren: int
) -> bool:
    """Return True if FL_NOEXCEPT / noexcept appears between ')' and body/';'."""
    close_line, close_col = _find_close_paren_multiline(lines, start, open_paren)
    if close_line < 0:
        return False  # can't find closing paren, skip

    # Check remainder of the line containing ')'
    tail = lines[close_line][close_col + 1 :]
    if _HAS_NOEXCEPT.search(tail):
        return True

    # Check up to 5 continuation lines
    for j in range(1, 6):
        idx = close_line + j
        if idx >= len(lines):
            break
        nxt = lines[idx].strip()
        if _HAS_NOEXCEPT.search(nxt):
            return True
        # Stop at statement/body terminators
        if nxt.startswith("{") or nxt.endswith("{") or nxt == "};":
            break
        if nxt.startswith(":") and not nxt.startswith("::"):
            break  # member-init list
        if nxt.endswith(";"):
            break

    return False


# ── Checker class ───────────────────────────────────────────────────────────


class NoexceptSpecialMembersChecker(FileContentChecker):
    """Lint checker: every special member function in src/fl/ must have FL_NOEXCEPT."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        p = file_path.replace("\\", "/")
        if not p.endswith((".h", ".hpp", ".cpp", ".cpp.hpp")):
            return False
        if "/src/fl/" not in p and not p.startswith("src/fl/"):
            return False
        if p.endswith("/noexcept.h") or p.endswith("fl/stl/noexcept.h"):
            return False
        if "/third_party/" in p:
            return False
        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        path = file_content.path
        lines = file_content.lines
        class_names: set[str] = set(_CLASS_DEF.findall(file_content.content))
        comment_mask = _compute_block_comment_mask(lines)

        for i, line in enumerate(lines):
            stripped = line.strip()
            if not stripped or stripped.startswith("//") or stripped.startswith("#"):
                continue
            if comment_mask[i]:
                continue
            if stripped.startswith("/*") or stripped.startswith("*"):
                continue
            if _SUPPRESS.search(line):
                continue

            info = classify_line(line, class_names)
            if info is None:
                # Try multi-line: join with subsequent lines and re-classify
                joined = _join_multiline_signature(lines, i)
                if joined:
                    info = classify_line(joined, class_names)
                    if info:
                        # Use the open-paren position from the original line
                        kind = info[0]
                        idx = line.index("(") if "(" in line else info[1]
                        info = (kind, idx)
            if info is None:
                continue
            kind, open_paren = info

            if signature_has_noexcept(lines, i, open_paren):
                continue

            self.violations.setdefault(path, []).append(
                (i + 1, f"Missing FL_NOEXCEPT on {kind}: {stripped}")
            )

        return []


# ── Auto-fixer ──────────────────────────────────────────────────────────────


def _insert_noexcept_at(line: str, close_paren_col: int) -> str:
    """Insert ' FL_NOEXCEPT' immediately after ')' at *close_paren_col*."""
    before = line[: close_paren_col + 1]
    after = line[close_paren_col + 1 :]
    # No trailing space needed when followed by ; or { directly
    if not after or after[0] in (";", "{"):
        return before + " FL_NOEXCEPT" + after
    # Already has a space → insert keyword before existing space
    if after[0] == " ":
        return before + " FL_NOEXCEPT" + after
    # Otherwise add a space after
    return before + " FL_NOEXCEPT " + after


def _ensure_include(content: str) -> str:
    """Ensure ``#include "fl/stl/noexcept.h"`` is present when FL_NOEXCEPT is used."""
    if "fl/stl/noexcept.h" in content:
        return content
    if "FL_NOEXCEPT" not in content:
        return content

    lines = content.split("\n")
    insert_idx = 0
    for i, ln in enumerate(lines):
        stripped = ln.strip()
        if stripped.startswith("#include"):
            insert_idx = i + 1
        elif stripped == "#pragma once" and insert_idx == 0:
            insert_idx = i + 1
    lines.insert(insert_idx, _NOEXCEPT_INCLUDE)
    return "\n".join(lines)


def fix_file(
    path: Path, dry_run: bool = False
) -> tuple[int, list[str]]:
    """Fix missing FL_NOEXCEPT in *path*.

    Returns ``(number_of_fixes, list_of_descriptions)``.
    """
    content = path.read_text(encoding="utf-8")
    lines = content.split("\n")
    class_names: set[str] = set(_CLASS_DEF.findall(content))
    comment_mask = _compute_block_comment_mask(lines)

    # (close_line_idx, close_col, kind)
    fixes: list[tuple[int, int, str]] = []
    skipped: list[tuple[int, str]] = []

    for i, line in enumerate(lines):
        stripped = line.strip()
        if not stripped or stripped.startswith("//") or stripped.startswith("#"):
            continue
        if comment_mask[i]:
            continue
        if stripped.startswith("/*") or stripped.startswith("*"):
            continue
        if _SUPPRESS.search(line):
            continue

        info = classify_line(line, class_names)
        if info is None:
            # Try multi-line
            joined = _join_multiline_signature(lines, i)
            if joined:
                info = classify_line(joined, class_names)
                if info:
                    kind_ml = info[0]
                    idx_ml = line.index("(") if "(" in line else info[1]
                    info = (kind_ml, idx_ml)
        if info is None:
            continue
        kind, open_paren = info

        if signature_has_noexcept(lines, i, open_paren):
            continue

        # Find the closing ) position for the fix
        cl, cc = _find_close_paren_multiline(lines, i, open_paren)
        if cl >= 0:
            fixes.append((cl, cc, kind))
        else:
            skipped.append((i + 1, kind))

    if not fixes and not skipped:
        return 0, []

    descriptions: list[str] = []
    for cl, cc, kind in fixes:
        verb = "Would add" if dry_run else "Add"
        descriptions.append(f"  Line {cl + 1}: {verb} FL_NOEXCEPT to {kind}")
    for line_num, kind in skipped:
        descriptions.append(f"  Line {line_num}: MANUAL: multi-line {kind}")

    if dry_run:
        return len(fixes) + len(skipped), descriptions

    # Apply fixes in reverse order to preserve positions
    for cl, cc, _kind in sorted(fixes, reverse=True):
        lines[cl] = _insert_noexcept_at(lines[cl], cc)

    new_content = "\n".join(lines)
    new_content = _ensure_include(new_content)
    path.write_text(new_content, encoding="utf-8")
    return len(fixes), descriptions


# ── CLI ─────────────────────────────────────────────────────────────────────


def main() -> int:
    fix_mode = "--fix" in sys.argv
    dry_run = "--dry-run" in sys.argv

    if fix_mode or dry_run:
        src_fl = PROJECT_ROOT / "src" / "fl"
        if not src_fl.exists():
            print("ERROR: src/fl/ not found", file=sys.stderr)
            return 1

        exts = {".h", ".hpp", ".cpp"}
        files = sorted(
            p
            for p in src_fl.rglob("*")
            if p.suffix in exts or p.name.endswith(".cpp.hpp")
        )
        files = [
            f
            for f in files
            if "/third_party/" not in str(f).replace("\\", "/")
            and not str(f).endswith("noexcept.h")
        ]

        total_fixes = 0
        for fp in files:
            n, descs = fix_file(fp, dry_run=dry_run)
            if n:
                label = "Would fix" if dry_run else "Fixed"
                print(f"{label} {fp.relative_to(PROJECT_ROOT)}: {n} violation(s)")
                for d in descs:
                    print(d)
                total_fixes += n
        verb = "Would fix" if dry_run else "Fixed"
        print(f"\n{verb} {total_fixes} violation(s) total")
        return 0

    # Lint mode
    from ci.util.check_files import run_checker_standalone

    checker = NoexceptSpecialMembersChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src" / "fl")],
        "Missing FL_NOEXCEPT on special member functions",
        extensions=[".h", ".hpp", ".cpp", ".cpp.hpp"],
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
