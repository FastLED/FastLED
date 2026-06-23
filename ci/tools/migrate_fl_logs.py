#!/usr/bin/env python3
"""Mechanical migration helper for legacy FastLED logging macros.

This handles straightforward stream chains by converting them to printf-style
`FL_*_F` calls with `%s` placeholders. It intentionally skips harder cases
such as stream manipulators so the linter can report them for manual cleanup.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
CPP_EXTS = {".cpp", ".h", ".hpp"}

MACROS: dict[str, str] = {
    "FL_WARN": "FL_WARN_F",
    "FL_WARN_IF": "FL_WARN_F_IF",
    "FL_WARN_ONCE": "FL_WARN_F_ONCE",
    "FL_WARN_EVERY": "FL_WARN_F_EVERY",
    "FL_WARN_FMT": "FL_WARN_F",
    "FL_WARN_FMT_IF": "FL_WARN_F_IF",
    "FL_PRINT": "FL_PRINT_F",
    "FL_PRINT_EVERY": "FL_PRINT_F_EVERY",
    "FL_DBG": "FL_DBG_F",
    "FL_DBG_IF": "FL_DBG_F_IF",
    "FL_DBG_EVERY": "FL_DBG_F_EVERY",
    "FL_ERROR": "FL_ERROR_F",
    "FL_ERROR_IF": "FL_ERROR_F_IF",
    "FL_LOG_SPI": "FL_LOG_SPI_F",
    "FL_LOG_RMT": "FL_LOG_RMT_F",
    "FL_LOG_PARLIO": "FL_LOG_PARLIO_F",
    "FL_LOG_AUDIO": "FL_LOG_AUDIO_F",
    "FL_LOG_INTERRUPT": "FL_LOG_INTERRUPT_F",
    "FL_LOG_FLEXIO": "FL_LOG_FLEXIO_F",
    "FL_LOG_OBJECTFLED": "FL_LOG_OBJECTFLED_F",
    "FL_LOG_ASYNC": "FL_LOG_ASYNC_F",
    "FL_LOG_SPI_ASYNC_MAIN": "FL_LOG_SPI_ASYNC_MAIN_F",
    "FL_LOG_RMT_ASYNC_MAIN": "FL_LOG_RMT_ASYNC_MAIN_F",
    "FL_LOG_PARLIO_ASYNC_MAIN": "FL_LOG_PARLIO_ASYNC_MAIN_F",
    "FL_LOG_AUDIO_ASYNC_MAIN": "FL_LOG_AUDIO_ASYNC_MAIN_F",
    "FL_LOG_INTERRUPT_ASYNC_MAIN": "FL_LOG_INTERRUPT_ASYNC_MAIN_F",
    "FL_LOG_FLEXIO_ASYNC_MAIN": "FL_LOG_FLEXIO_ASYNC_MAIN_F",
    "FL_LOG_OBJECTFLED_ASYNC_MAIN": "FL_LOG_OBJECTFLED_ASYNC_MAIN_F",
}

_SORTED_MACRO_KEYS: list[str] = sorted(MACROS, key=lambda s: len(s), reverse=True)
MACRO_RE = re.compile(
    r"\b(" + "|".join(re.escape(name) for name in _SORTED_MACRO_KEYS) + r")\s*\("
)


def iter_files(paths: list[Path]) -> list[Path]:
    files: list[Path] = []
    for path in paths:
        if path.is_file() and path.suffix in CPP_EXTS:
            files.append(path)
            continue
        if path.is_dir():
            files.extend(p for p in path.rglob("*") if p.suffix in CPP_EXTS)
    return sorted(set(files))


def find_matching_paren(text: str, open_index: int) -> int:
    depth = 0
    quote: str | None = None
    escape = False
    in_line_comment = False
    in_block_comment = False
    i = open_index
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if in_line_comment:
            if ch == "\n":
                in_line_comment = False
            i += 1
            continue
        if in_block_comment:
            if ch == "*" and nxt == "/":
                in_block_comment = False
                i += 2
                continue
            i += 1
            continue
        if quote:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == quote:
                quote = None
            i += 1
            continue
        if ch == "/" and nxt == "/":
            in_line_comment = True
            i += 2
            continue
        if ch == "/" and nxt == "*":
            in_block_comment = True
            i += 2
            continue
        if ch in {"'", '"'}:
            quote = ch
            i += 1
            continue
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def split_top_level(text: str, delimiter: str) -> list[str]:
    parts: list[str] = []
    start = 0
    depth = 0
    quote: str | None = None
    escape = False
    i = 0
    while i < len(text):
        ch = text[i]
        if quote:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == quote:
                quote = None
            i += 1
            continue
        if ch in {"'", '"'}:
            quote = ch
            i += 1
            continue
        if depth == 0 and text.startswith(delimiter, i):
            parts.append(text[start:i].strip())
            i += len(delimiter)
            start = i
            continue
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth = max(0, depth - 1)
        i += 1
    parts.append(text[start:].strip())
    return parts


def string_literal_payload(expr: str) -> str | None:
    expr = expr.strip()
    if len(expr) < 2 or expr[0] != '"' or expr[-1] != '"':
        return None
    if len(expr) >= 3 and expr[-2] == "\\":
        return None
    return expr[1:-1]


def convert_stream_expr(expr: str) -> str | None:
    expr = expr.strip()
    if "fl::hex" in expr or "fl::dec" in expr:
        return None
    parts = split_top_level(expr, "<<")
    if not parts:
        return None

    fmt_parts: list[str] = []
    args: list[str] = []
    for part in parts:
        payload = string_literal_payload(part)
        if payload is not None:
            fmt_parts.append(payload)
        else:
            fmt_parts.append("%s")
            args.append(part)

    fmt = '"' + "".join(fmt_parts).replace('"', '\\"') + '"'
    if args:
        return fmt + ", " + ", ".join(args)
    return fmt


def convert_args(macro: str, args_text: str) -> str | None:
    if macro in {"FL_WARN_IF", "FL_WARN_FMT_IF", "FL_DBG_IF", "FL_ERROR_IF"}:
        args = split_top_level(args_text, ",")
        if len(args) != 2:
            return None
        converted = convert_stream_expr(args[1])
        return f"{args[0]}, {converted}" if converted else None
    if macro in {"FL_WARN_EVERY", "FL_PRINT_EVERY", "FL_DBG_EVERY"}:
        args = split_top_level(args_text, ",")
        if len(args) != 2:
            return None
        converted = convert_stream_expr(args[1])
        return f"{args[0]}, {converted}" if converted else None
    if macro == "FL_LOG_ASYNC":
        args = split_top_level(args_text, ",")
        if len(args) != 2:
            return None
        converted = convert_stream_expr(args[1])
        return f"{args[0]}, {converted}" if converted else None
    converted = convert_stream_expr(args_text)
    return converted


def convert_file(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    output: list[str] = []
    cursor = 0
    changes = 0
    for match in MACRO_RE.finditer(text):
        macro = match.group(1)
        if macro.endswith("_F"):
            continue
        line_start = text.rfind("\n", 0, match.start()) + 1
        line_prefix = text[line_start : match.start()]
        if line_prefix.lstrip().startswith("//") or line_prefix.lstrip().startswith(
            "#define"
        ):
            continue
        open_paren = text.find("(", match.start())
        close_paren = find_matching_paren(text, open_paren)
        if close_paren < 0:
            continue
        args_text = text[open_paren + 1 : close_paren]
        converted_args = convert_args(macro, args_text)
        if converted_args is None:
            continue
        output.append(text[cursor : match.start()])
        output.append(f"{MACROS[macro]}({converted_args})")
        cursor = close_paren + 1
        changes += 1
    if changes:
        output.append(text[cursor:])
        path.write_text("".join(output), encoding="utf-8", newline="")
    return changes


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("paths", nargs="*", default=[str(SRC)])
    args = parser.parse_args()
    total = 0
    for path in iter_files([Path(p) for p in args.paths]):
        if path.resolve() == (SRC / "fl" / "log" / "log.h").resolve():
            continue
        total += convert_file(path)
    print(f"Converted {total} legacy log call(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
