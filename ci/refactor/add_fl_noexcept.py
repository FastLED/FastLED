#!/usr/bin/env python3
"""Add FL_NO_EXCEPT to functions using clang-query AST + multi-line-aware insertion.

Uses clang-query to find the exact file:line of every function missing noexcept,
then reads the file context to find the balanced closing paren across multiple
lines, skips past cv-qualifiers, and inserts FL_NO_EXCEPT at the correct position.

Handles cases the single-line inserter cannot:
  - Multi-line function signatures
  - SFINAE enable_if return types
  - constexpr functions
  - Attribute macros before function body
  - Template functions spanning multiple lines

Usage:
    uv run python ci/refactor/add_fl_noexcept.py --scope fl          # dry-run
    uv run python ci/refactor/add_fl_noexcept.py --scope fl --apply  # apply
    uv run python ci/refactor/add_fl_noexcept.py --scope platforms   # all platforms
"""

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

from ci.tools.check_noexcept import _SCOPES as CHECK_SCOPES
from ci.tools.check_noexcept import NoexceptCheckError, find_missing_noexcept


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
_NOEXCEPT_INCLUDE = '#include "fl/stl/noexcept.h"'
_SCOPES = tuple(CHECK_SCOPES)


def _discover_hits(scope: str) -> list[tuple[str, int]]:
    """Use active lint discovery and exclusions without applying its debt baseline."""
    return sorted({(hit.path, hit.line) for hit in find_missing_noexcept(scope)})


# ============================================================================
# Step 3: Multi-line-aware FL_NO_EXCEPT insertion
# ============================================================================


def _find_balanced_close_paren(lines: list[str], start_line: int) -> tuple[int, int]:
    """Find function parameters, skipping balanced prefix attribute/type expressions."""
    prefix_expressions = {
        "__attribute__",
        "__declspec",
        "decltype",
        "sizeof",
        "alignof",
        "alignas",
        "__alignof__",
        "typeof",
        "__typeof__",
        "_Pragma",
    }
    line_index = start_line
    column = 0
    while line_index < min(start_line + 30, len(lines)):
        opening = lines[line_index].find("(", column)
        if opening < 0:
            line_index += 1
            column = 0
            continue
        preceding = (
            "\n".join(lines[start_line:line_index]) + "\n" + lines[line_index][:opening]
        )
        word = re.search(r"([A-Za-z_]\w*)\s*$", preceding)
        closing = _find_balanced_close_paren_from(lines, line_index, opening)
        if closing == (-1, -1):
            return closing
        if word is None or word.group(1) not in prefix_expressions:
            return closing
        line_index, column = closing
        column += 1
    return (-1, -1)


def _find_balanced_close_paren_from(
    lines: list[str], start_line: int, start_col: int
) -> tuple[int, int]:
    """Find the balanced closing ')' starting from '(' at (start_line, start_col).

    Returns (line_index, col_index) of the matching ')' or (-1, -1).
    """
    depth = 0
    for li in range(start_line, min(start_line + 30, len(lines))):
        begin = start_col if li == start_line else 0
        for ci in range(begin, len(lines[li])):
            ch = lines[li][ci]
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    return (li, ci)
    return (-1, -1)


def _skip_cv_and_trailing_return(
    lines: list[str], line_idx: int, col: int
) -> tuple[int, int]:
    """Find the specification position after cv/ref qualifiers and before ->.

    Handles: ) const volatile && -> decltype(...) {
    Returns (line_idx, col) pointing to the insertion position for FL_NO_EXCEPT.
    """
    pos_line = line_idx
    pos_col = col + 1  # start after ')'

    # Build a view of text after the close paren (up to 5 lines)
    text_parts: list[str] = []
    line_offsets: list[tuple[int, int]] = []  # (line_idx, start_col_in_text)
    offset = 0

    for li in range(pos_line, min(pos_line + 5, len(lines))):
        start_col = pos_col if li == pos_line else 0
        segment = lines[li][start_col:]
        line_offsets.append((li, offset))
        text_parts.append(segment)
        offset += len(segment) + 1  # +1 for the join newline

    text = "\n".join(text_parts)
    rest = text.lstrip()
    consumed = len(text) - len(rest)

    specification_end = 0

    # Skip const/volatile
    for qualifier in ("const", "volatile"):
        if rest.startswith(qualifier) and (
            len(rest) == len(qualifier) or not rest[len(qualifier)].isalnum()
        ):
            consumed += len(qualifier)
            specification_end = consumed
            rest = text[consumed:].lstrip()
            consumed = len(text) - len(rest)

    # Exception specifications precede trailing return types and follow
    # ref-qualifiers. Never scan through -> into a return-type expression.
    for qualifier in ("&&", "&"):
        if rest.startswith(qualifier):
            consumed += len(qualifier)
            specification_end = consumed
            rest = text[consumed:].lstrip()
            consumed = len(text) - len(rest)
            break

    def _offset_to_line_col(consumed_offset: int) -> tuple[int, int]:
        running = 0
        for idx, part in enumerate(text_parts):
            li = line_offsets[idx][0]
            sc = pos_col if li == pos_line else 0
            if running + len(part) >= consumed_offset:
                return (li, sc + (consumed_offset - running))
            running += len(part) + 1
        return (pos_line, pos_col)

    # A directive between parameters and initializer belongs on its own line.
    # Put the specification on the preceding signature, never before #if.
    if rest.startswith("#"):
        return _offset_to_line_col(specification_end)
    return _offset_to_line_col(consumed)


def _insert_fl_noexcept_multiline(
    lines: list[str], start_line: int
) -> list[tuple[int, str]] | None:
    """Insert FL_NO_EXCEPT into a function signature starting at start_line.

    Returns list of (line_index, new_line_content) replacements, or None if
    insertion is not possible or not needed.
    """
    # Build the signature text from start_line through the end of the
    # function signature (up to the first ; or { after balanced parens).
    # Stop precisely at the terminator to avoid picking up FL_NO_EXCEPT
    # from a subsequent function on a nearby line.
    sig_text = ""
    depth = 0
    found_open = False
    sig_done = False
    for li in range(start_line, min(start_line + 30, len(lines))):
        for ci, ch in enumerate(lines[li]):
            sig_text += ch
            if ch == "(":
                depth += 1
                found_open = True
            elif ch == ")":
                depth -= 1
            elif ch in ("{", ";") and found_open and depth == 0:
                sig_done = True
                break
        if sig_done:
            break
        sig_text += " "

    if re.search(r"\b(?:FL_NO_EXCEPT|noexcept)\b", sig_text):
        return None

    # Skip destructors
    if re.search(r"~\w+\s*\(", sig_text):
        return None

    # Find the balanced close paren
    close_line, close_col = _find_balanced_close_paren(lines, start_line)
    if close_line < 0:
        return None

    # For operator(), the first balanced () is the operator name, not the
    # parameters. Check if text up to the close paren ends with "operator()".
    # If so, advance to find the actual parameter parens.
    text_up_to_close = lines[close_line][: close_col + 1]
    if close_line > start_line:
        # Multi-line: join text from start to close
        parts = [lines[li] for li in range(start_line, close_line)]
        parts.append(text_up_to_close)
        text_up_to_close = " ".join(parts)
    is_operator_call_parens = bool(
        re.search(r"\boperator\s*\(\s*\)\s*$", text_up_to_close)
    )
    if is_operator_call_parens:
        # Advance past the name paren and find the parameter paren
        param_start_line = close_line
        param_start_col = close_col + 1
        # Find the next '(' after the name ')'
        found_param_paren = False
        for li in range(param_start_line, min(param_start_line + 5, len(lines))):
            start = param_start_col if li == param_start_line else 0
            for ci in range(start, len(lines[li])):
                if lines[li][ci] == "(":
                    # Found the parameter paren, now find its close
                    close_line, close_col = _find_balanced_close_paren_from(
                        lines, li, ci
                    )
                    found_param_paren = True
                    break
            if found_param_paren:
                break
        if not found_param_paren or close_line < 0:
            return None

    # Skip past cv-qualifiers
    insert_line, insert_col = _skip_cv_and_trailing_return(lines, close_line, close_col)

    # Get the line we're inserting into
    target_line = lines[insert_line]
    prefix = target_line[:insert_col]
    before = prefix.rstrip() if prefix.strip() else prefix
    separator = " " if before.strip() else ""
    after = target_line[insert_col:].lstrip()

    # Determine what follows and build insertion
    if not after:
        new_line = before + separator + "FL_NO_EXCEPT"
    elif after[0] == ";":
        new_line = before + separator + "FL_NO_EXCEPT;" + after[1:]
    elif after[0] == "{":
        new_line = before + separator + "FL_NO_EXCEPT {" + after[1:]
    elif after.startswith("override") or after.startswith("final"):
        new_line = before + separator + "FL_NO_EXCEPT " + after
    elif after.startswith("__attribute__"):
        new_line = before + separator + "FL_NO_EXCEPT " + after
    elif after.startswith(":"):
        # Constructor initializer list
        new_line = before + separator + "FL_NO_EXCEPT " + after
    else:
        new_line = before + separator + "FL_NO_EXCEPT " + after

    return [(insert_line, new_line)]


# ============================================================================
# Step 4: Ensure include
# ============================================================================


def _ensure_include(lines: list[str]) -> bool:
    """Add #include "fl/stl/noexcept.h" if not present. Returns True if added."""
    content = "\n".join(lines)
    if _NOEXCEPT_INCLUDE in content:
        return False

    last_include = -1
    for i, line in enumerate(lines):
        if line.strip().startswith("#include"):
            last_include = i

    if last_include >= 0:
        lines.insert(last_include + 1, _NOEXCEPT_INCLUDE)
    else:
        for i, line in enumerate(lines):
            if "#pragma once" in line:
                lines.insert(i + 1, _NOEXCEPT_INCLUDE)
                return True
        lines.insert(0, _NOEXCEPT_INCLUDE)

    return True


# ============================================================================
# Step 5: Apply fixes
# ============================================================================


@dataclass(slots=True)
class FixResult:
    """Result of applying FL_NO_EXCEPT fixes."""

    changes: int = 0
    files: int = 0
    skipped: int = 0


def _apply_fixes(hits: list[tuple[str, int]], apply: bool) -> FixResult:
    """Apply FL_NO_EXCEPT insertions."""
    by_file: dict[str, list[int]] = {}
    for filepath, line_num in hits:
        by_file.setdefault(filepath, []).append(line_num)

    total_changes = 0
    total_files = 0
    total_skipped = 0

    for filepath in sorted(by_file):
        full_path = PROJECT_ROOT / filepath
        if not full_path.exists():
            continue

        content = full_path.read_text(encoding="utf-8", errors="replace")
        lines = content.split("\n")
        file_changes = 0
        file_skipped = 0

        # Collect all replacements first, then apply in reverse order
        all_replacements: list[
            tuple[int, str, int]
        ] = []  # (line_idx, new_content, original_line_num)

        for line_num in sorted(by_file[filepath]):
            if line_num < 1 or line_num > len(lines):
                continue

            result = _insert_fl_noexcept_multiline(lines, line_num - 1)
            if result is not None:
                for line_idx, new_content in result:
                    all_replacements.append((line_idx, new_content, line_num))
                file_changes += 1
            else:
                file_skipped += 1

        if file_changes > 0:
            if not apply:
                # Dry-run: show what would change
                for line_idx, new_content, orig_num in all_replacements:
                    print(f"  {filepath}:{orig_num}")
                    print(f"    - {lines[line_idx].strip()}")
                    print(f"    + {new_content.strip()}")
            else:
                # Apply in reverse to preserve line numbers
                for line_idx, new_content, _ in sorted(
                    all_replacements, key=lambda x: x[0], reverse=True
                ):
                    lines[line_idx] = new_content

                include_added = _ensure_include(lines)
                # split("\n") retains the final empty item, so joining already
                # preserves the original newline without adding a blank line.
                full_path.write_text("\n".join(lines), encoding="utf-8")
                print(
                    f"  {filepath}: {file_changes} changes"
                    f"{' (+include)' if include_added else ''}"
                )

            total_files += 1

        total_changes += file_changes
        total_skipped += file_skipped

    return FixResult(changes=total_changes, files=total_files, skipped=total_skipped)


# ============================================================================
# Main
# ============================================================================


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Add FL_NO_EXCEPT to functions using clang-query AST analysis "
            "with multi-line-aware insertion."
        )
    )
    parser.add_argument(
        "--scope",
        choices=_SCOPES,
        default="all",
        help="Which code scope to process (default: all)",
    )
    parser.add_argument(
        "--apply",
        action="store_true",
        help="Apply changes (default: dry-run)",
    )
    args = parser.parse_args()

    print(f"Scope: {args.scope}")
    print(f"Mode:  {'apply' if args.apply else 'dry-run'}")
    print()

    try:
        all_hits = _discover_hits(args.scope)
    except NoexceptCheckError as exc:
        print(f"ERROR: {exc}")
        return 1

    if not all_hits:
        print("\nAll functions have FL_NO_EXCEPT. Nothing to do.")
        return 0

    print(f"Found {len(all_hits)} functions missing FL_NO_EXCEPT")
    print()

    result = _apply_fixes(all_hits, args.apply)

    print()
    verb = "Applied" if args.apply else "Would apply"
    print(
        f"{verb}: {result.changes} changes across {result.files} files ({result.skipped} skipped)"
    )

    if not args.apply and result.changes > 0:
        print("\nRe-run with --apply to write changes.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
