#!/usr/bin/env python3
"""Deterministic instruction-count measurement of the MP3 decoders, via Callgrind.

Wall-clock has produced two published-and-retracted numbers in this project
(machine load once, a configuration mismatch once). Callgrind counts
instructions, so it cannot be wrong that way: the Helix total lands within 0.2%
of a baseline recorded a year ago on different hardware.

Read the two warnings before quoting anything this prints.

  1. **The device is the authority.** Host and device have disagreed by 2.5x on
     this decoder, and have disagreed in *direction* at least once: a one-copy
     lane-stepped polyphase measured 231.1M here and 47,665 us on an ESP32-C6,
     while a two-copy version measured 236.7M here and 46,505 us there. x86-64
     at -O2 inlines and unrolls things a riscv32 at -Os does not. Use this to
     find where the instructions are and to prove a change is worth trying; use
     `bash autoresearch esp32c6 --mp3` to decide whether it shipped a win.
  2. This builds the *scalar* path (-DMINIMP3_NO_SIMD), because that is what
     every embedded target runs. Numbers here do not describe the SSE/NEON
     kernels.

Usage
    uv run python ci/codec_cpu/callgrind.py
    uv run python ci/codec_cpu/callgrind.py --baseline HEAD
    uv run python ci/codec_cpu/callgrind.py --baseline HEAD --per-file
    uv run python ci/codec_cpu/callgrind.py --decoder minimp3-float --top 8

`--baseline REF` measures the working tree and `REF`'s version of
src/third_party/minimp3/ side by side and prints the delta. That is the shape
almost every question about this decoder takes: "did my edit help, and where".
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / ".build" / "codec-callgrind"

# The decoder under test lives entirely in this directory, so a baseline can be
# materialised by checking out just these files at a ref rather than standing up
# a whole worktree.
VENDORED = "src/third_party/minimp3"

# Pinned to match ci/codec_cpu/audit.py's host configuration. Do not "clean
# these up": -O2 with inlining is what ships, and the -fno-inline figure that
# an earlier audit reported understates the gap because Helix gains more from
# inlining than minimp3 does.
FLAGS = [
    "-Isrc",
    "-Isrc/platforms/stub",
    "-std=gnu++11",
    # -Os, not -O2: the ESP32-C6 and every other target this decoder ships to
        # build at -Os, and the two are not interchangeable here. -O3 on the
        # polyphase produces 395 memory operations against a hand-written 188,
        # and -O2 sits between. Profiling an optimisation level nobody ships
        # measures a different program.
        "-Os",
    "-fno-exceptions",
    "-fno-rtti",
    "-fno-strict-aliasing",
    "-DMINIMP3_NO_SIMD",
    "-DFASTLED_USE_PROGMEM=0",
    "-DSTUB_PLATFORM",
    "-DARDUINO=10808",
    "-DFASTLED_USE_STUB_ARDUINO",
    "-DFASTLED_STUB_IMPL",
    "-DFASTLED_TESTING",
    "-DFASTLED_NO_AUTO_NAMESPACE",
    "-DFASTLED_NO_PINMAP",
    "-g",
]

DECODERS = ("minimp3-fixed", "minimp3-float", "helix")

# Same corpus the CPU audit uses, and it matters that the mono vector is in it:
# mp3d_synth runs a different lane path for one channel, and a corpus of only
# stereo content is blind to a third of that stream's decode time.
CORPUS = (
    "tests/data/codec/minimp3/l3-hecommon.bit",
    "tests/data/codec/minimp3/l3-compl-cut.mp3",
    "tests/data/codec/minimp3/l3-he_free.bit",
    "tests/data/codec/minimp3/l3-lame-vbrtag.bit",
    "tests/data/codec/minimp3/M2L3_bitrate_16_all.bit",
)

# A single-file driver, for --per-file. The shipping ci/codec_cpu/driver.cpp
# decodes the whole corpus in one process and takes no path argument; splitting
# a total across files after the fact is not possible, and attributing a change
# to "the mono stream" is exactly the question that keeps coming up.
PER_FILE_DRIVER = r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fl/codec/mp3_memory.h"

#define MINIMP3_FIXED_POINT 1
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#include "third_party/minimp3/minimp3.h"

namespace fl {
void* memcpy(void* d, const void* s, fl::size n) FL_NO_EXCEPT { return ::memcpy(d, s, n); }
void* memmove(void* d, const void* s, fl::size n) FL_NO_EXCEPT { return ::memmove(d, s, n); }
void* memset(void* d, int v, fl::size n) FL_NO_EXCEPT { return ::memset(d, v, n); }
namespace third_party {
void* Mp3MemoryAllocate(fl::size b, Mp3MemoryTag) FL_NO_EXCEPT { return ::malloc(b); }
void Mp3MemoryFree(void* p, fl::size, Mp3MemoryTag) FL_NO_EXCEPT { ::free(p); }
}
}

using namespace fl::third_party;

/* An FNV-1a over the emitted PCM, printed alongside the frame count. A speedup
   measured over fewer frames is not a speedup, and this project has published
   one of those; the checksum is what makes "bit-exact" checkable rather than
   asserted. */
int main(int argc, char** argv) {
    static mp3dec_t dec;
    static mp3dec_scratch_t scratch;
    for (int a = 1; a < argc; ++a) {
        FILE* f = fopen(argv[a], "rb");
        if (!f) { fprintf(stderr, "missing %s\n", argv[a]); return 1; }
        fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
        unsigned char* buf = (unsigned char*)malloc(n > 0 ? (size_t)n : 1);
        if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); return 1; }
        fclose(f);
        mp3dec_init(&dec);
        const unsigned char* p = buf;
        int left = (int)n;
        mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
        uint64_t sum = 1469598103934665603ull;
        int frames = 0, channels = 0, hz = 0;
        while (left > 0) {
            mp3dec_frame_info_t info;
            int samples = mp3dec_decode_frame_r(&dec, &scratch, p, left, pcm, &info);
            if (info.frame_bytes <= 0) break;
            p += info.frame_bytes;
            left -= info.frame_bytes;
            if (samples) {
                ++frames;
                channels = info.channels;
                hz = info.hz;
                for (int i = 0; i < samples * info.channels; ++i) {
                    sum = (sum ^ (uint16_t)pcm[i]) * 1099511628211ull;
                }
            }
        }
        printf("DECODE frames=%d channels=%d hz=%d fnv=%016llx\n",
               frames, channels, hz, (unsigned long long)sum);
        free(buf);
    }
    return 0;
}
"""


def helix_available() -> bool:
    """Is the RPSL/RCSL reference decoder present?

    It normally is not: it lives outside this tree by design. Everything that
    matters -- minimp3 totals, per-function attribution, baseline deltas --
    works without it, so its absence downgrades the report rather than failing
    it.
    """
    return (ROOT / "ci" / "codec_cpu" / "helix_driver.cpp").exists() and (
        ROOT / "src" / "third_party" / "libhelix_mp3" / "_build.cpp.hpp"
    ).exists()


def _no_functions() -> list[tuple[int, str]]:
    """Typed default for Attribution.functions; a bare `list` reads as unknown."""
    return []


@dataclass
class Attribution:
    total: int = 0
    functions: list[tuple[int, str]] = field(default_factory=_no_functions)


def _run(command: list[str], cwd: Path = ROOT) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command, cwd=cwd, check=True, text=True, capture_output=True
    )


def _require(tool: str) -> str:
    path = shutil.which(tool)
    if not path:
        raise SystemExit(f"{tool} is required; install it or enter the toolchain env")
    return path


def materialise_baseline(ref: str, into: Path) -> Path:
    """Check out just the vendored decoder at `ref` into a shadow include root.

    Returned as a `-I` prefix rather than applied to the working tree: the
    caller almost always has uncommitted work, and `git stash` around a
    long-running measurement is how a half-measured tree gets shipped.
    """
    shadow = into / "third_party" / "minimp3"
    shadow.mkdir(parents=True, exist_ok=True)
    listing = _run(["git", "ls-tree", "--name-only", f"{ref}:{VENDORED}"])
    names = [n for n in listing.stdout.split() if n]
    if not any(n == "minimp3.h" for n in names):
        raise SystemExit(f"{ref}:{VENDORED} has no minimp3.h")
    for name in names:
        blob = subprocess.run(
            ["git", "show", f"{ref}:{VENDORED}/{name}"],
            cwd=ROOT,
            check=True,
            capture_output=True,
        )
        (shadow / name).write_bytes(blob.stdout)
    return into


def compile_driver(out_dir: Path, include_first: Path | None, per_file: bool) -> Path:
    clang = _require("clang++")
    out_dir.mkdir(parents=True, exist_ok=True)
    prefix = ["-I" + str(include_first)] if include_first else []
    binary = out_dir / ("perfile" if per_file else "drv")

    if per_file:
        source = out_dir / "perfile.cpp"
        source.write_text(PER_FILE_DRIVER)
        _run([clang, *prefix, *FLAGS, str(source), "-o", str(binary)])
        return binary

    # The Helix reference decoder is optional and usually absent: it is
    # RPSL/RCSL and lives outside this tree. When it is not here, build without
    # it rather than failing -- the minimp3 numbers and the per-function
    # attribution are the point, and the helix column is a bonus.
    objects: list[str] = []
    units = {
        "f.o": "ci/codec_memory/minimp3_audit.cpp",
        "x.o": "ci/codec_cpu/minimp3_fixed_driver.cpp",
    }
    flags = list(FLAGS)
    if helix_available():
        units["h.o"] = "ci/codec_cpu/helix_driver.cpp"
        flags.append("-DFL_CODEC_CPU_HAS_HELIX=1")
    for name, unit in units.items():
        obj = out_dir / name
        _run([clang, *prefix, *flags, "-c", unit, "-o", str(obj)])
        objects.append(str(obj))
    _run([clang, *prefix, *flags, "ci/codec_cpu/driver.cpp", *objects, "-o", str(binary)])
    return binary


_IREFS = re.compile(r"I\s+refs:\s+([\d,]+)")
# `callgrind_annotate` prints "<count> (<pct>) <file>:<signature> [<binary>]".
# The file part is matched as "no spaces and no colons" rather than "\S+": a
# greedy \S+ backtracks to the last colon in the C++ signature and slices the
# name off mid-argument-list.
# callgrind_annotate emits one row per (file, function) pair, so a function
# whose body was inlined from a second header appears twice: once as
# `minimp3.h:mp3d_synth_granule ... [/path/to/binary]` and once as
# `int_asm.h:mp3d_synth_granule` with no trailing binary marker.
#
# Requiring that trailing `\[` -- which the first version of this regex did --
# silently kept only the first row. For minimp3-fixed the dropped half was every
# fl::math::mul_shift_round32 the decoder inlines: 12.3M under
# mp3d_synth_granule and 7.5M under mp3dec_decode_frame_r. Helix splits the same
# way (assembly.h against polyphase.hpp / dct32.hpp / imdct.hpp) but happened to
# be read with both halves summed, so the two decoders were not being measured
# the same way and two conclusions drawn from it were backwards.
#
# Keep every row and aggregate by function name below.
_ANNOTATED = re.compile(
    # `\(\s*[\d.]+%\)` -- the whitespace matters. callgrind_annotate
    # right-aligns the percentage, so anything under 10% arrives as "( 6.71%)"
    # with a leading space. Without that `\s*` the pattern silently matched only
    # functions above 10%, which on this decoder is exactly three of them, and
    # `--top 12` returning 3 rows was the visible symptom.
    r"^\s*([\d,]+)\s+\(\s*[\d.]+%\)\s+([^\s:]+):([^\[\n]+)",
    re.MULTILINE,
)


def callgrind(binary: Path, args: list[str], out: Path, top: int) -> Attribution:
    valgrind = _require("valgrind")
    result = subprocess.run(
        [
            valgrind,
            "--tool=callgrind",
            f"--callgrind-out-file={out}",
            "--instr-atstart=yes",
            str(binary),
            *args,
        ],
        cwd=ROOT,
        check=True,
        text=True,
        capture_output=True,
    )
    match = _IREFS.search(result.stderr)
    if not match:
        raise SystemExit("callgrind produced no instruction count")
    attribution = Attribution(total=int(match.group(1).replace(",", "")))

    annotate = shutil.which("callgrind_annotate")
    if annotate and top:
        text = subprocess.run(
            # 99, not 90: callgrind_annotate's threshold is a *cumulative*
            # percentage, so at 90 it stops emitting rows once the running
            # total reaches 90% of the program -- which silently caps the list
            # at three or four functions regardless of --top. Asking for the
            # top 12 and getting 3 is what that looks like.
            [annotate, "--threshold=99", str(out)],
            cwd=ROOT,
            check=False,
            text=True,
            capture_output=True,
        ).stdout
        found: list[tuple[int, str]] = []
        # Aggregate by function, not by (file, function): an inlined body
        # split across two headers is one function's cost, and reporting half
        # of it makes decoders with different header layouts incomparable.
        totals: dict[str, int] = {}
        breakdown: dict[str, dict[str, int]] = {}
        for count, source, symbol in _ANNOTATED.findall(text):
            symbol = symbol.strip()
            if not symbol or symbol.startswith("<"):
                continue
            # Strip the namespace and the argument list; what is wanted is a
            # name short enough to sit in a table.
            name = symbol.split("(")[0].split("::")[-1]
            value = int(count.replace(",", ""))
            totals[name] = totals.get(name, 0) + value
            breakdown.setdefault(name, {})[source] = (
                breakdown.setdefault(name, {}).get(source, 0) + value
            )
        found = [(value, name) for name, value in totals.items()]
        found.sort(reverse=True)
        attribution.functions = found[:top]
    return attribution


def _pct(new: int, old: int) -> str:
    if not old:
        return "n/a"
    return f"{100.0 * (new - old) / old:+.2f}%"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--baseline",
        metavar="REF",
        help="also measure this git ref's src/third_party/minimp3 and print the delta",
    )
    parser.add_argument(
        "--decoder",
        default="minimp3-fixed",
        choices=DECODERS,
        help="which decoder the whole-corpus run drives (default: minimp3-fixed)",
    )
    parser.add_argument(
        "--no-helix",
        action="store_true",
        help="skip the Helix reference run (halves the runtime)",
    )
    parser.add_argument(
        "--per-file",
        action="store_true",
        help="one fixed-point decode per corpus file, with frames and a PCM checksum",
    )
    parser.add_argument("--top", type=int, default=5, help="functions to attribute")
    args = parser.parse_args(argv)

    BUILD.mkdir(parents=True, exist_ok=True)
    variants: list[tuple[str, Path | None]] = [("tree", None)]
    if args.baseline:
        shadow = materialise_baseline(args.baseline, BUILD / "baseline-src")
        variants.append((args.baseline, shadow))

    totals: dict[str, int] = {}
    helix_totals: dict[str, int] = {}
    per_file: dict[str, dict[str, tuple[int, str]]] = {}
    attributions: dict[str, Attribution] = {}

    for label, include in variants:
        out_dir = BUILD / label.replace("/", "_")
        if args.per_file:
            binary = compile_driver(out_dir, include, per_file=True)
            rows: dict[str, tuple[int, str]] = {}
            for relative in CORPUS:
                run = callgrind(
                    binary, [relative], out_dir / "cg.perfile", top=0
                )
                detail = subprocess.run(
                    [str(binary), relative], cwd=ROOT, check=True,
                    text=True, capture_output=True,
                ).stdout.strip()
                rows[relative] = (run.total, detail.replace("DECODE ", ""))
            per_file[label] = rows
            continue

        binary = compile_driver(out_dir, include, per_file=False)
        attributions[label] = callgrind(
            binary, [args.decoder], out_dir / "cg.out", args.top
        )
        totals[label] = attributions[label].total
        if not args.no_helix and helix_available():
            helix_totals[label] = callgrind(
                binary, ["helix"], out_dir / "cg.helix", top=0
            ).total

    if args.per_file:
        print(f"{'stream':44s} {'Ir':>14s}", end="")
        if args.baseline:
            print(f" {'baseline':>14s} {'delta':>9s}", end="")
        print("  decode")
        for relative in CORPUS:
            total, detail = per_file["tree"][relative]
            print(f"{Path(relative).name:44s} {total:14,d}", end="")
            if args.baseline:
                base = per_file[args.baseline][relative][0]
                print(f" {base:14,d} {_pct(total, base):>9s}", end="")
            print(f"  {detail}")
            if args.baseline:
                base_detail = per_file[args.baseline][relative][1]
                if base_detail != detail:
                    print(f"{'':44s} {'':14s} !! baseline decoded {base_detail}")
        return 0

    print(f"{'variant':10s} {'decoder':>14s} {'Ir':>14s}", end="")
    if not args.no_helix and helix_available():
        print(f" {'helix':>14s} {'ratio':>8s}", end="")
    print()
    for label, _ in variants:
        print(f"{label:10s} {args.decoder:>14s} {totals[label]:14,d}", end="")
        if not args.no_helix and helix_available():
            helix = helix_totals[label]
            print(f" {helix:14,d} {totals[label] / helix:7.3f}x", end="")
        print()
    if args.baseline:
        delta = totals["tree"] - totals[args.baseline]
        print(f"{'delta':10s} {'':>14s} {delta:14,d} "
              f"({_pct(totals['tree'], totals[args.baseline])})")

    tree = attributions["tree"]
    print(f"\ntop {args.top} functions (tree, {args.decoder}):")
    for count, name in tree.functions:
        share = 100.0 * count / tree.total if tree.total else 0.0
        line = f"  {share:5.1f}%  {count:14,d}  {name}"
        if args.baseline:
            before = dict(
                (n, c) for c, n in attributions[args.baseline].functions
            ).get(name)
            if before is not None:
                line += f"   ({_pct(count, before)})"
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
