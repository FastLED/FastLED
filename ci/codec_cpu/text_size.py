#!/usr/bin/env python3
"""riscv32 `.text` for the fixed-point decoder, optionally per function.

Every optimisation round so far has traded code size for speed -- +14% then
+3.9% -- and that budget is not unlimited on the parts this decoder ships to.
The figure was being produced by hand-building a cross-compiled translation unit
each time; this does it in one command so it can be quoted per change like the
Callgrind numbers are.

    uv run python ci/codec_cpu/text_size.py
    uv run python ci/codec_cpu/text_size.py --baseline HEAD --functions
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TU = ROOT / "ci" / "codec_cpu" / "minimp3_fixed_codegen.cpp"

FLAGS = [
    "-std=gnu++11", "-Os", "-fno-exceptions", "-fno-rtti", "-fno-strict-aliasing",
    "-DFL_CODEC_CPU_CODEGEN_ESP_TYPES", "-DMINIMP3_NO_SIMD",
    "-DFASTLED_USE_PROGMEM=0", "-DARDUINO=10808", "-DFASTLED_NO_AUTO_NAMESPACE",
]


def _toolchain() -> tuple[str, str]:
    found = list(Path.home().glob(
        ".platformio/packages/**/bin/riscv32-esp-elf-g++"))
    if not found:
        raise SystemExit(
            "riscv32-esp-elf-g++ not found; install it with\n"
            "  uv run pio pkg install -g -t "
            "'espressif/toolchain-riscv32-esp@12.2.0+20230208'"
        )
    gpp = str(found[0])
    return gpp, str(Path(gpp).with_name("riscv32-esp-elf-objdump"))


def _compile(gpp: str, out: Path, include_first: Path | None) -> Path:
    obj = out / "codec.o"
    includes = ([f"-I{include_first}"] if include_first else []) + [
        f"-I{ROOT / 'src'}", f"-I{ROOT / 'src' / 'platforms' / 'stub'}"
    ]
    subprocess.run(
        [gpp, *includes, *FLAGS, "-c", str(TU), "-o", str(obj)],
        check=True, capture_output=True,
    )
    return obj


def _sections(objdump: str, obj: Path) -> dict[str, int]:
    out = subprocess.run([objdump, "-h", str(obj)], capture_output=True,
                         text=True, check=True).stdout
    sizes: dict[str, int] = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) > 2 and parts[1].startswith("."):
            try:
                sizes[parts[1]] = int(parts[2], 16)
            except ValueError:
                continue
    return sizes


def _functions(objdump: str, obj: Path) -> dict[str, int]:
    out = subprocess.run([objdump, "-t", str(obj)], capture_output=True,
                         text=True, check=True).stdout
    sizes: dict[str, int] = {}
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+\s+.*?\s+F\s+\.text\s+([0-9a-f]+)\s+(\S+)", line)
        if m:
            name = m.group(2)
            demangled = name.split("E")[0] if name.startswith("_Z") else name
            sizes[demangled] = int(m.group(1), 16)
    return sizes


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", help="git ref to compare against")
    parser.add_argument("--functions", action="store_true",
                        help="per-function .text, largest first")
    parser.add_argument("--top", type=int, default=12)
    args = parser.parse_args(argv)

    gpp, objdump = _toolchain()

    with tempfile.TemporaryDirectory() as tmp:
        now = _compile(gpp, Path(tmp), None)
        now_sections = _sections(objdump, now)
        print(f"  .text  {now_sections.get('.text', 0):>9,} bytes")
        for name in (".rodata", ".data", ".bss"):
            if now_sections.get(name):
                print(f"  {name:<6} {now_sections[name]:>9,} bytes")

        if args.baseline:
            shadow = Path(tmp) / "baseline"
            (shadow / "third_party").mkdir(parents=True)
            subprocess.run(
                ["git", "archive", args.baseline, "src/third_party/minimp3"],
                cwd=ROOT, check=True, stdout=subprocess.PIPE,
            )
            tar = subprocess.run(
                ["git", "archive", args.baseline, "src/third_party/minimp3"],
                cwd=ROOT, check=True, capture_output=True,
            ).stdout
            subprocess.run(["tar", "-x", "-C", str(shadow), "--strip-components=1"],
                           input=tar, check=True)
            base = _compile(gpp, Path(tmp), shadow)
            base_text = _sections(objdump, base).get(".text", 0)
            delta = now_sections.get(".text", 0) - base_text
            pct = (delta / base_text * 100) if base_text else 0.0
            print(f"  baseline {args.baseline}: {base_text:,} bytes")
            print(f"  delta    {delta:+,} bytes ({pct:+.2f}%)")

        if args.functions:
            print("\n  per-function .text:")
            for size, name in sorted(
                ((v, k) for k, v in _functions(objdump, now).items()), reverse=True
            )[: args.top]:
                print(f"    {size:>7,}  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
