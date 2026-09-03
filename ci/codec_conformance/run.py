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

# The suite is fixed at a pinned revision, so the count is knowable. Requiring
# it exactly stops a partial cache or a failed extraction from reporting a
# green run over a subset.
EXPECTED_PAIRS = 74

# Every bitstream in the suite, including the nine that ship no reference PCM.
# The sanitizer pass must see all of them: the malformed streams are precisely
# the ones without references, and `l3-nonstandard-big-iscf` -- the vector that
# found the polyphase overflow in FastLED#4133 -- is one of them. Guarding that
# pass with EXPECTED_PAIRS accepted a 74-stream cache and skipped exactly the
# inputs the pass exists to cover.
EXPECTED_BITSTREAMS = 83

# A decoder can emit a correct prefix and then stop, and a PSNR taken over the
# shared prefix would not notice. So a shortfall against the reference is a
# failure unless it is listed here with a reason.
#
# Every entry below is a Layer I or Layer II vector whose reference PCM is a
# fixed-size power-of-two buffer -- 32768, 65536, 131072, 163840 samples --
# rather than a frame-exact dump, so "produced < reference" does not mean
# dropped audio for these. Two independent checks say the same: the shortfall
# is identical to the sample on the *float* build, and inserting a resync on
# parse failure recovers nothing. FastLED ships Layer III; Layer I/II decode is
# incidental, and no Layer III vector is on this list.
# The exact shortfall is recorded per vector, not just the name. A name-only
# allowlist would let a listed vector truncate by any amount at all -- which is
# the same hole the length check was added to close, reopened for the fifteen
# vectors that need an exception. These numbers are identical on the fixed and
# float builds, which is the evidence that they are a property of the reference
# files rather than of the fixed-point port.
KNOWN_SHORT_OUTPUT = {
    "l1-fl1": 27904,
    "l1-fl2": 27904,
    "l1-fl3": 27904,
    "l1-fl4": 13952,
    "l1-fl5": 27904,
    "l1-fl6": 27904,
    "l1-fl7": 17152,
    "l1-fl8": 27904,
    "l2-fl10": 18176,
    "l2-fl11": 18176,
    "l2-fl12": 18176,
    "l2-fl13": 9088,
    "l2-fl14": 28672,
    "l2-fl15": 28672,
    "l2-fl16": 18688,
}

# Decoders legitimately run a frame or two past the reference (encoder delay and
# the final granule), which the repo's own tests allow as the "standard vector
# length". Beyond that, surplus output is a defect unless listed.
STANDARD_LENGTH_ALLOWANCE = 2 * 1152 * 2  # two frames, stereo

# Bounded the same way as KNOWN_SHORT_OUTPUT, and for the same reason: a
# name-only entry would let a listed vector overrun by any amount at all, and
# the surplus is just as invisible to a PSNR taken over the shared prefix as a
# shortfall is. The number below is measured and identical on the fixed and
# float builds.
KNOWN_LONG_OUTPUT = {
    # Sample rate switches mid-stream (44.1 -> 48 kHz). The decoder follows the
    # switch and emits 172800 samples more than the reference, which captures
    # only one rate. Scores 132.27 dB over the shared prefix.
    "l3-nonstandard-he_44_48khz": 172800,
}

_SINGLE_FRAME_REASON = (
    "A single-frame stream of 274-2048 bytes. minimp3 will not decode a lone "
    "frame without lookahead to validate it, and upstream's float build "
    "produces zero frames on these too -- so it is neither a regression nor "
    "specific to the fixed-point port. Not a tag-handling problem: a real "
    "multi-frame file with an ID3v2, APE or ID3v1 tag attached decodes "
    "byte-identically to the untagged original (FastLED#4129)."
)

# Vectors that do not clear the floor for reasons that are not decoder defects.
# Each needs a reason; an empty reason is not acceptable.
#
# Empty as of FastLED#4129. The LAME VBR-tag vector used to live here because
# the decode loop emitted the Xing header frame as audio and ignored the
# encoder delay; it now skips both and scores 108.22 dB.
KNOWN_EXCEPTIONS: dict[str, str] = {}

# Vectors that ship a non-empty reference but legitimately decode to nothing.
#
# Without this list `no_overlap` is a free pass: a change that stopped decoding
# a real vector would move it into "without comparable reference" and the run
# would still report green. Anything landing there unlisted is a failure.
KNOWN_NO_OUTPUT = {
    "l3-nonstandard-vbrtag-oob-read": (
        "104 bytes containing nothing but an Info tag frame. Since FastLED#4129 "
        "that frame is recognised as metadata and not emitted, so the file "
        "yields no audio -- which is the point of the change. Its reference is "
        "2304 samples of pure silence, i.e. the tag frame decoded as if it were "
        "audio; the sibling tag-only vectors vbrtag-only and vbrtag-empty ship "
        "*empty* references, so upstream's own expectation for a tag-only file "
        "is no output."
    ),
    "l3-nonstandard-id3v1": _SINGLE_FRAME_REASON,
    "l3-nonstandard-id3v2": _SINGLE_FRAME_REASON,
    "l3-nonstandard-apetag": _SINGLE_FRAME_REASON,
    "l3-nonstandard-id3v1-apetag": _SINGLE_FRAME_REASON,
}


@typechecked
@dataclass(frozen=True)
class VectorResult:
    name: str
    status: str
    psnr: float
    layer: int
    frames: int
    produced: int
    reference: int


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


def build_harness(*, float_variant: bool = False, sanitize: bool = False) -> Path:
    BUILD.mkdir(parents=True, exist_ok=True)
    suffix = "float" if float_variant else "fixed"
    if sanitize:
        suffix += "_san"
    binary = BUILD / f"harness_{suffix}"
    command = [
        str(compiler_path()),
        f"-I{ROOT / 'src'}",
        f"-I{ROOT / 'src' / 'platforms' / 'stub'}",
        "-std=gnu++11",
        "-O1" if sanitize else "-O2",
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
    if sanitize:
        # UBSan is the point: the fixed-point port does int32 arithmetic that
        # ordinary audio never pushes to its limits, so a malformed stream is
        # the only thing that exercises the edges. FastLED#4133 was exactly
        # that -- a signed overflow in the polyphase, invisible on every
        # well-formed vector.
        command += ["-g", "-fno-omit-frame-pointer", "-fsanitize=address,undefined"]
    if float_variant:
        command.append("-DCONFORMANCE_FLOAT")
    command += [str(Path(__file__).with_name("harness.cpp")), "-o", str(binary)]
    RunningProcess.run(command, check=True)
    return binary


_RESULT_RE = re.compile(
    r"RESULT status=(\w+)(?:.*?psnr=([-\d.]+))?(?:.*?layer=(\d+))?"
    r"(?:.*?frames=(\d+))?(?:.*?produced=(\d+))?(?:.*?reference=(\d+))?"
)


_SANITIZER_RE = re.compile(r"(AddressSanitizer|runtime error:|SEGV)")


def run_vector(binary: Path, bitstream: Path, reference: Path) -> VectorResult:
    environment = dict(os.environ)
    # Leaks are not what this pass is looking for and the harness exits without
    # freeing on some paths by design.
    environment["ASAN_OPTIONS"] = "detect_leaks=0"
    result = RunningProcess.run(
        [str(binary), str(bitstream), str(reference)],
        cwd=ROOT,
        check=False,
        text=True,
        capture_output=True,
        timeout=180,
        env=environment,
    )
    combined = (result.stdout or "") + (result.stderr or "")
    if _SANITIZER_RE.search(combined):
        detail = next(
            (line for line in combined.splitlines() if _SANITIZER_RE.search(line)),
            "sanitizer finding",
        )
        print(f"  SANITIZER {bitstream.stem}: {detail.strip()[:160]}")
        return VectorResult(bitstream.stem, "sanitizer", 0.0, 0, 0, 0, 0)
    match = _RESULT_RE.search(result.stdout or "")
    if not match:
        return VectorResult(bitstream.stem, "crashed", 0.0, 0, 0, 0, 0)
    return VectorResult(
        name=bitstream.stem,
        status=match.group(1),
        psnr=float(match.group(2)) if match.group(2) else 0.0,
        layer=int(match.group(3)) if match.group(3) else 0,
        frames=int(match.group(4)) if match.group(4) else 0,
        produced=int(match.group(5)) if match.group(5) else 0,
        reference=int(match.group(6)) if match.group(6) else 0,
    )


def collect_for_sanitizer(vectors: Path) -> list[tuple[Path, Path]]:
    """Every bitstream, reference or not.

    The PSNR pass can only use vectors that ship a reference PCM. The sanitizer
    pass has no such constraint and must not inherit it: the malformed streams
    are exactly the ones with no reference, and they are the only inputs that
    push the fixed-point arithmetic to its edges. `l3-nonstandard-big-iscf`,
    which found FastLED#4133, has no reference -- so scoping this pass to the
    74 comparable pairs would have excluded the one vector that mattered.

    A reference is still passed because the harness takes two arguments; which
    one is irrelevant here, since only sanitizer output is inspected.
    """
    placeholder = next(iter(sorted(vectors.glob("*.pcm"))), None)
    if placeholder is None:
        raise RuntimeError("no reference PCM available to drive the harness")
    streams: list[tuple[Path, Path]] = []
    for pattern in ("*.bit", "*.mp3"):
        for bitstream in sorted(vectors.glob(pattern)):
            own = bitstream.with_suffix(".pcm")
            streams.append((bitstream, own if own.exists() else placeholder))
    return streams


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
    parser.add_argument(
        "--sanitize",
        action="store_true",
        help="build with ASan/UBSan and require every vector to be clean",
    )
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args(argv)

    try:
        vectors = fetch_vectors()
        binary = build_harness(float_variant=args.float, sanitize=args.sanitize)
        if args.sanitize:
            streams = collect_for_sanitizer(vectors)
            if len(streams) != EXPECTED_BITSTREAMS:
                raise RuntimeError(
                    f"expected exactly {EXPECTED_BITSTREAMS} bitstreams to "
                    f"sanitize at {UPSTREAM_REVISION[:8]}, found "
                    f"{len(streams)}; a cache holding only the "
                    f"{EXPECTED_PAIRS} reference-backed vectors would skip the "
                    f"malformed streams this pass exists to cover"
                )
            findings = 0
            for bitstream, reference in streams:
                outcome = run_vector(binary, bitstream, reference)
                if outcome.status == "sanitizer":
                    findings += 1
            print(
                f"CONFORMANCE-SANITIZE: {len(streams)} bitstreams, "
                f"{findings} sanitizer findings"
            )
            if findings:
                raise RuntimeError(f"{findings} bitstreams tripped ASan or UBSan")
            print("CONFORMANCE:PASS")
            return 0

        pairs = collect(vectors)
        if len(pairs) != EXPECTED_PAIRS:
            raise RuntimeError(
                f"expected exactly {EXPECTED_PAIRS} conformance pairs at "
                f"{UPSTREAM_REVISION[:8]}, found {len(pairs)}; a partial cache "
                "or a changed revision would otherwise pass over a subset"
            )

        failures: list[VectorResult] = []
        excepted: list[VectorResult] = []
        passed = 0
        no_overlap = 0
        for bitstream, reference in pairs:
            outcome = run_vector(binary, bitstream, reference)
            if outcome.status == "no_overlap":
                if outcome.name not in KNOWN_NO_OUTPUT:
                    # Silently tolerating this would let a change that stopped
                    # decoding a real vector pass as "nothing to compare".
                    failures.append(outcome)
                    continue
                no_overlap += 1
                continue
            if outcome.status != "ok":
                failures.append(outcome)
                continue
            # Length before PSNR: a decoder that emits a correct prefix and
            # then truncates scores well on the shared prefix, so the PSNR gate
            # alone cannot see it.
            shortfall = outcome.reference - outcome.produced
            surplus = outcome.produced - outcome.reference
            if shortfall > 0 and shortfall != KNOWN_SHORT_OUTPUT.get(outcome.name):
                failures.append(outcome)
                continue
            if surplus > STANDARD_LENGTH_ALLOWANCE and surplus != (
                KNOWN_LONG_OUTPUT.get(outcome.name)
            ):
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
                f"psnr={outcome.psnr:.2f} dB layer={outcome.layer} "
                f"produced={outcome.produced} reference={outcome.reference}"
            )
        if failures:
            raise RuntimeError(
                f"{len(failures)} conformance vectors below the "
                f"{ISO_FLOOR_DB:.0f} dB ISO floor"
            )
        print("CONFORMANCE:PASS")
    except KeyboardInterrupt:  # noqa: KBI002 - top-level handler, exits 130 cleanly
        print("CONFORMANCE:INTERRUPTED", file=sys.stderr)
        return 130
    except (OSError, RuntimeError, subprocess.CalledProcessError, ValueError) as exc:
        print(f"codec conformance failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
