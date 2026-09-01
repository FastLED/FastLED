#!/usr/bin/env python3
"""Run the full MPEG audio conformance suite against the shipping MP3 decoder.

The suite is 83 vector/reference pairs and 19 MB of PCM, which is more than
belongs in a library people clone, so the vectors are fetched from a pinned
upstream revision and cached rather than vendored. One vector -- the one that
found FastLED#4127 -- is vendored under tests/data so that regression is caught
by `bash test` without a network.

Every vector that ships a reference must clear the ISO limited-accuracy floor
of 60 dB. Known exceptions are listed explicitly below rather than skipped
silently.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tarfile
import urllib.request
from dataclasses import dataclass
from pathlib import Path

from running_process import RunningProcess
from typeguard import typechecked


ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / ".build" / "codec-conformance"
CACHE = ROOT / ".cache" / "codec-conformance"

# Pinned to the revision recorded in the component manifest, so the suite is
# reproducible and cannot drift under the gate.
UPSTREAM_REVISION = "ea99364f61c14656440e8d77e9c233ccf3124633"
TARBALL = f"https://codeload.github.com/lieff/minimp3/tar.gz/{UPSTREAM_REVISION}"

ISO_FLOOR_DB = 60.0

# Vectors that do not clear the floor for reasons that are not decoder defects.
# Each needs a reason; an empty reason is not acceptable.
KNOWN_EXCEPTIONS = {
    "l3-nonstandard-sin1k0db_lame_vbrtag": (
        "LAME VBR tag frame. The raw mp3dec_decode_frame_r loop this harness "
        "and fl::Mp3Decoder both use does not skip the Xing/LAME header frame "
        "or apply its encoder delay, so the output is offset against a "
        "reference that assumes gapless trimming. Upstream's own float build "
        "scores identically (2.85 dB), so this is a container-metadata gap in "
        "the caller, not an arithmetic defect. Tracked separately."
    ),
}


@typechecked
@dataclass(frozen=True)
class VectorResult:
    name: str
    status: str
    psnr: float
    layer: int
    frames: int


def fetch_vectors() -> Path:
    """Download and unpack the upstream conformance vectors, cached."""
    target = CACHE / UPSTREAM_REVISION / "vectors"
    if target.is_dir() and any(target.glob("*.pcm")):
        return target
    target.parent.mkdir(parents=True, exist_ok=True)
    archive = target.parent / "minimp3.tar.gz"
    if not archive.exists():
        print(f"fetching conformance vectors from {UPSTREAM_REVISION[:8]}...")
        with urllib.request.urlopen(TARBALL) as response:
            archive.write_bytes(response.read())
    prefix = f"minimp3-{UPSTREAM_REVISION}/vectors/"
    with tarfile.open(archive, "r:gz") as tar:
        members = [m for m in tar.getmembers() if m.name.startswith(prefix)]
        if not members:
            raise RuntimeError("upstream tarball has no vectors/ directory")
        for member in members:
            member.name = member.name[len(prefix) :]
            if member.name:
                tar.extract(member, target, filter="data")
    return target


def compiler_path() -> Path:
    system = shutil.which("clang++")
    if system:
        return Path(system)
    raise RuntimeError("clang++ is required for the conformance harness")


def build_harness(*, float_variant: bool = False) -> Path:
    BUILD.mkdir(parents=True, exist_ok=True)
    suffix = "float" if float_variant else "fixed"
    binary = BUILD / f"harness_{suffix}"
    command = [
        str(compiler_path()),
        f"-I{ROOT / 'src'}",
        f"-I{ROOT / 'src' / 'platforms' / 'stub'}",
        "-std=gnu++11",
        "-O2",
        "-fno-exceptions",
        "-fno-rtti",
        "-fno-strict-aliasing",
        "-DFASTLED_USE_PROGMEM=0",
        "-DSTUB_PLATFORM",
        "-DARDUINO=10808",
        "-DFASTLED_USE_STUB_ARDUINO",
        "-DFASTLED_STUB_IMPL",
        "-DFASTLED_TESTING",
        "-DFASTLED_NO_AUTO_NAMESPACE",
        "-DFASTLED_NO_PINMAP",
    ]
    if float_variant:
        command.append("-DCONFORMANCE_FLOAT")
    command += [str(Path(__file__).with_name("harness.cpp")), "-o", str(binary)]
    RunningProcess.run(command, check=True)
    return binary


_RESULT_RE = re.compile(
    r"RESULT status=(\w+)(?:.*?psnr=([-\d.]+))?(?:.*?layer=(\d+))?"
    r"(?:.*?frames=(\d+))?"
)


def run_vector(binary: Path, bitstream: Path, reference: Path) -> VectorResult:
    result = RunningProcess.run(
        [str(binary), str(bitstream), str(reference)],
        cwd=ROOT,
        check=False,
        text=True,
        capture_output=True,
        timeout=120,
    )
    match = _RESULT_RE.search(result.stdout or "")
    if not match:
        return VectorResult(bitstream.stem, "crashed", 0.0, 0, 0)
    return VectorResult(
        name=bitstream.stem,
        status=match.group(1),
        psnr=float(match.group(2)) if match.group(2) else 0.0,
        layer=int(match.group(3)) if match.group(3) else 0,
        frames=int(match.group(4)) if match.group(4) else 0,
    )


def collect(vectors: Path) -> list[tuple[Path, Path]]:
    pairs: list[tuple[Path, Path]] = []
    for reference in sorted(vectors.glob("*.pcm")):
        if reference.stat().st_size == 0:
            continue
        for extension in (".bit", ".mp3"):
            candidate = reference.with_suffix(extension)
            if candidate.exists():
                pairs.append((candidate, reference))
                break
    return pairs


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--float", action="store_true", help="audit the float build")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args(argv)

    try:
        vectors = fetch_vectors()
        binary = build_harness(float_variant=args.float)
        pairs = collect(vectors)
        if len(pairs) < 60:
            raise RuntimeError(
                f"expected the full conformance suite, found {len(pairs)} pairs"
            )

        failures: list[VectorResult] = []
        excepted: list[VectorResult] = []
        passed = 0
        no_overlap = 0
        for bitstream, reference in pairs:
            outcome = run_vector(binary, bitstream, reference)
            if outcome.status == "no_overlap":
                # Tag-only or empty-reference vectors: nothing to compare.
                no_overlap += 1
                continue
            if outcome.status != "ok":
                failures.append(outcome)
                continue
            if outcome.psnr >= ISO_FLOOR_DB:
                passed += 1
                if args.verbose:
                    print(f"  PASS {outcome.name:38s} {outcome.psnr:8.2f} dB")
            elif outcome.name in KNOWN_EXCEPTIONS:
                excepted.append(outcome)
            else:
                failures.append(outcome)

        print(
            f"CONFORMANCE: {passed} passed, {len(failures)} failed, "
            f"{len(excepted)} known-excepted, {no_overlap} without comparable "
            f"reference, of {len(pairs)} vectors"
        )
        for outcome in excepted:
            print(f"  EXCEPTED {outcome.name}: {outcome.psnr:.2f} dB")
            print(f"           {KNOWN_EXCEPTIONS[outcome.name]}")
        for outcome in failures:
            print(
                f"  FAIL {outcome.name}: status={outcome.status} "
                f"psnr={outcome.psnr:.2f} dB layer={outcome.layer}"
            )
        if failures:
            raise RuntimeError(
                f"{len(failures)} conformance vectors below the "
                f"{ISO_FLOOR_DB:.0f} dB ISO floor"
            )
        print("CONFORMANCE:PASS")
    except (OSError, RuntimeError, subprocess.CalledProcessError, ValueError) as exc:
        print(f"codec conformance failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
