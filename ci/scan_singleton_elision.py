#!/usr/bin/env python3
"""
Scan the FastLED tree for namespace-scope variables that should be wrapped
in `fl::Singleton<T>` for `--gc-sections` elision. Mirrors the logic that
`ci/lint_cpp_rs/src/checkers/singleton_elision.rs` will implement once the
Rust linter integration is fully wired.

Report shape:
  - Per-file violation count (top-N)
  - Whole-tree total
  - Bucketed by root directory (src/fl vs src/platforms vs src/fx vs etc.)
  - Full violation list to a JSON file for detailed review

Usage:
  uv run ci/scan_singleton_elision.py
"""

from __future__ import annotations

import json
import re
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SRC_ROOT = ROOT / "src"

# TU sources only — headers (`.h`, `.hpp` without `.cpp.hpp`) are a separate
# concern caught by StaticInHeaderChecker.
EXTS = (".cpp.hpp", ".cpp", ".cc", ".cxx")

EXCLUDED_DIRS = {
    "third_party",
    "third-party",
}

# The Singleton template itself IS the implementation of the pattern; its
# storage lives inside `instance()` and is elidable. Exclude to avoid the
# meta-noise.
EXCLUDED_FILES = {
    "src/fl/stl/singleton.h",
    "src/fl/stl/singleton.cpp.hpp",
}


TRIVIAL_RHS_RE = re.compile(
    r"^\s*("
    r"[-+]?0[xX][0-9a-fA-F]+[uUlL]*"  # hex literal
    r"|[-+]?0[bB][01]+[uUlL]*"  # binary literal
    r"|[-+]?\d+[uUlL]*"  # int literal
    r"|[-+]?\d*\.\d+[fF]?"  # float literal
    r"|nullptr|NULL|true|false"  # symbolic constants
    r"|\".*\""  # string literal
    r"|'.'"  # char literal
    r"|[A-Z_][A-Z0-9_]*"  # single all-caps macro
    r"|\(\s*[-+]?0[xX][0-9a-fA-F]+[uUlL]*\s*<<\s*\d+\s*\)"  # `(1u << N)` bit-shifts common in register bitmasks
    r"|\(\s*[-+]?\d+[uUlL]*\s*<<\s*\d+\s*\)"  # ditto for decimal base
    r")\s*$"
)


def strip_comments(line: str, in_block: bool) -> tuple[str, bool]:
    """Strip `/* … */` and `// …` comment text. Returns (code, in_block_after)."""
    out = ""
    i = 0
    n = len(line)
    while i < n:
        if in_block:
            end = line.find("*/", i)
            if end == -1:
                return out, True
            i = end + 2
            in_block = False
        else:
            start = line.find("/*", i)
            line_comment = line.find("//", i)
            if start != -1 and (line_comment == -1 or start < line_comment):
                out += line[i:start]
                i = start + 2
                in_block = True
            elif line_comment != -1:
                out += line[i:line_comment]
                return out, False
            else:
                out += line[i:]
                return out, False
    return out, in_block


NAMESPACE_OPEN_RE = re.compile(r"^\s*namespace\s+\w+\s*\{")
NAMESPACE_ANONYMOUS_RE = re.compile(r"^\s*namespace\s*\{")


def looks_like_var_def(code: str) -> tuple[bool, str, str]:  # noqa: DCT002
    """Return (is_definition, name, type_pretty) for a candidate line."""
    stripped = code.strip()
    if not stripped:
        return False, "", ""
    if stripped.startswith("//") or stripped.startswith("#"):
        return False, "", ""

    # Non-var starts.
    NON_VAR_STARTS = (
        "using ",
        "typedef ",
        "namespace ",
        "class ",
        "struct ",
        "enum ",
        "union ",
        "template<",
        "template ",
        "friend ",
        "public:",
        "private:",
        "protected:",
        "extern ",
        "return ",
        "return;",
        "//",
        "/*",
        "@",
        "if ",
        "if(",
        "else",
        "for ",
        "for(",
        "while ",
        "while(",
        "do ",
        "do{",
        "switch ",
        "switch(",
    )
    for prefix in NON_VAR_STARTS:
        if stripped.startswith(prefix):
            return False, "", ""

    # Must end with `;`.
    if not stripped.endswith(";"):
        return False, "", ""

    # Braces on the line? Then it's not a pure decl.
    if "{" in code or "}" in code:
        return False, "", ""

    # `(` before `;` → function decl / call / initializer, skip conservatively.
    if "(" in code:
        paren = code.find("(")
        semi = code.rfind(";")
        if paren < semi:
            return False, "", ""

    # Opt-outs.
    if "[[gnu::used]]" in code or "__attribute__((used))" in code or "FL_KEEP" in code:
        return False, "", ""

    # Constexpr with a trivial RHS = compile-time literal → skip.
    is_constexpr = (
        stripped.startswith("constexpr ")
        or stripped.startswith("static constexpr ")
        or stripped.startswith("inline constexpr ")
    )
    if is_constexpr and "=" in code:
        rhs = code[code.find("=") + 1 :].rstrip(";").strip()
        if TRIVIAL_RHS_RE.match(rhs):
            return False, "", ""

    # Extract name — token before first `=` / `;` / `[`.
    end_pos = min(
        (code.find(c) for c in "=;[" if code.find(c) != -1), default=len(code)
    )
    head = code[:end_pos].rstrip()

    # Class-static member out-of-class definitions like
    # `constexpr bool numeric_limits<char>::is_signed;` — the `::` in the
    # name path is the giveaway. C++11 requires these; they don't emit
    # storage unless ODR-used. Skip.
    if "::" in head:
        return False, "", ""
    # Name = last identifier in head.
    m = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*$", head)
    if not m:
        return False, "", ""
    name = m.group(1)
    if name.lower() in (
        "const",
        "volatile",
        "static",
        "inline",
        "constexpr",
        "auto",
        "register",
    ):
        return False, "", ""

    # Very-short candidates like a single character usually aren't real vars.
    if len(name) < 2 and name not in ("g",):
        return False, "", ""

    # Type = whatever preceded the name in head, minus storage-class keywords.
    type_part = head[: m.start()].strip()
    for kw in ("static ", "inline ", "constexpr ", "static constexpr "):
        while type_part.startswith(kw):
            type_part = type_part[len(kw) :]
    if not type_part:
        # Something like `MyType foo = ...` where MyType has no space? Unusual.
        return False, "", ""

    return True, name, type_part


def scan_file(path: Path) -> list[tuple[int, str, str]]:
    """Return list of (line_no, name, type) violations."""
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return []

    violations: list[tuple[int, str, str]] = []
    depth = 0
    namespace_stack: list[int] = []  # depths that are inside a `namespace X {`
    in_block_comment = False

    for idx, raw in enumerate(lines):
        code, in_block_comment = strip_comments(raw, in_block_comment)

        stripped = code.strip()

        # Detect a namespace-opening line for the depth we're ABOUT to enter.
        opens_namespace = bool(
            NAMESPACE_OPEN_RE.match(code) or NAMESPACE_ANONYMOUS_RE.match(code)
        )

        # Depth at start of line (before consuming this line's braces) is
        # what we check against.
        line_depth = depth

        # Update depth from this line's braces + track namespace opens.
        for ch in code:
            if ch == "{":
                depth += 1
                if opens_namespace and depth not in namespace_stack:
                    namespace_stack.append(depth)
            elif ch == "}":
                if depth in namespace_stack:
                    namespace_stack.remove(depth)
                depth -= 1
                if depth < 0:
                    depth = 0

        # Namespace-scope means line_depth == 0 (TU scope) or line_depth is
        # in namespace_stack (we're inside `namespace X { … }`).
        if line_depth == 0 or line_depth in namespace_stack:
            is_def, name, type_pretty = looks_like_var_def(code)
            if is_def:
                violations.append((idx + 1, name, type_pretty))

    return violations


def rel_key(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def main() -> int:
    files: list[Path] = []
    for ext in EXTS:
        files.extend(SRC_ROOT.rglob(f"*{ext}"))
    files = sorted(set(files))

    print(f"scanning {len(files)} files under {SRC_ROOT.relative_to(ROOT)}", flush=True)

    per_file: dict[str, list[tuple[int, str, str]]] = {}
    per_bucket: dict[str, int] = defaultdict(int)
    total = 0

    for path in files:
        rel = rel_key(path)
        if rel in EXCLUDED_FILES:
            continue
        parts = rel.split("/")
        if any(seg in EXCLUDED_DIRS for seg in parts):
            continue

        v = scan_file(path)
        if not v:
            continue
        per_file[rel] = v
        total += len(v)
        # Bucket by top two path segments after `src/`.
        rest = parts[1:]  # drop `src`
        bucket_seg = rest[0] if rest else "?"
        if len(rest) >= 2 and bucket_seg in {"platforms", "fl", "fx", "sensors"}:
            bucket_seg = f"{bucket_seg}/{rest[1]}"
        per_bucket[bucket_seg] += len(v)

    print()
    print(f"total namespace-scope variable definitions flagged: {total}")
    print(f"files touched: {len(per_file)}")
    print()

    print("=== TOP FILES BY VIOLATION COUNT ===")
    top_files = sorted(per_file.items(), key=lambda kv: -len(kv[1]))
    for rel, v in top_files[:40]:
        print(f"  {len(v):4d}  {rel}")
    if len(top_files) > 40:
        print(f"  … {len(top_files) - 40} more files")
    print()

    print("=== VIOLATIONS BY BUCKET ===")
    for bucket, count in sorted(per_bucket.items(), key=lambda kv: -kv[1]):
        print(f"  {count:4d}  src/{bucket}")
    print()

    # Detailed dump.
    out = ROOT / "singleton_elision_violations.json"
    with out.open("w", encoding="utf-8") as fh:
        json.dump(
            {
                "total": total,
                "files": {
                    rel: [{"line": ln, "name": n, "type": t} for ln, n, t in vs]
                    for rel, vs in per_file.items()
                },
                "buckets": dict(per_bucket),
            },
            fh,
            indent=2,
            sort_keys=True,
        )
    print(f"detailed JSON dump: {out.relative_to(ROOT)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
