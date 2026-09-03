#!/usr/bin/env python3
"""Time the MP3 decoder on attached hardware, with a variance estimate.

`bash autoresearch <board> --mp3` reports one number. One number is not a
measurement: this project has already published two speed figures that were
wrong, one of them because a single run landed on a loaded machine. Repeating
the run and reporting the spread is what makes a small delta trustworthy -- the
last accepted change was 4.6%, which a single pair of runs cannot distinguish
from noise without knowing the noise.

Also handles two local mechanics that otherwise waste a cycle each time:
a stale fbuild daemon (which fails with a misleading "port is busy" that is
really EACCES) and the dialout group.
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path

from running_process import RunningProcess
from typeguard import typechecked


ROOT = Path(__file__).resolve().parents[2]

# "Layer III only -- minimp3 46512 us, helix 35084 us  ->  minimp3-fixed is 1.33x helix"
RE_L3 = re.compile(
    r"Layer III only -- minimp3 (\d+) us, helix (\d+) us.*?is ([\d.]+)x helix"
)
RE_DECODE = re.compile(r"decode (\d+) us for (\d+) us of audio -> ([\d.]+)x real time")
RE_SUPPRESSED = re.compile(r"not the same work, ratio suppressed")
RE_PASS = re.compile(r"MP3 CODEC TEST PASSED")
RE_FNV = re.compile(r"combined_fnv1a=0x([0-9a-f]+)")
# The board re-enumerates on reset, so a back-to-back run can open a port
# the previous run left behind and hear nothing. That is the serial link
# failing, not the decoder, and it must not be reported as a result or
# stop a measurement loop -- it is the most common way this script fails.
RE_TRANSPORT = re.compile(
    r"MP3 CODEC TEST TIMEOUT|No response from device|"
    r"port is busy|could not open port|Device not found"
)


def _kill_stale_daemon() -> None:
    """A daemon started before the dialout group was granted cannot open the
    port, and reports it as "port is busy or doesn't exist" -- which sends you
    looking for a stuck process rather than at credentials."""
    found = RunningProcess.run(
        ["pgrep", "-x", "fbuild-daemon"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    for pid in found.stdout.split():
        # Output is discarded, so only the exit status matters here.
        RunningProcess.run(
            ["kill", pid],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
        )


@typechecked
@dataclass(frozen=True)
class Measurement:
    """One device run. Named and typed, so callers stop writing
    int(r["l3_us"]) and the checksum cannot be silently absent."""

    # The Layer III-only leg, and nothing else. It exists only when the Helix
    # reference is compiled in (the feat/helix-benchmark-reference branch;
    # Helix is RPSL/RCSL-licensed and does not ship on master), because that
    # is the firmware that times the two decoders over the same 8 frames.
    #
    # It deliberately does *not* fall back to decode_us when the line is
    # absent. The two differ by 13% on this fixture, so a transcript whose
    # Helix line was truncated on the serial link would otherwise hand back a
    # whole-fixture time wearing the Layer III label -- and with --runs 1
    # there is no second run to contradict it. Absent means absent; callers
    # pick decode_us, which is always the whole fixture.
    l3_us: int | None
    helix_us: int | None
    ratio: float | None
    decode_us: int
    audio_us: int
    realtime: float
    fnv1a: str


TRANSPORT_FAILURE = "transport"


# `board` is interpolated into a shell string below -- `sg dialout -c` takes a
# command line, not an argv -- so it is constrained to what a board name can
# actually be rather than trusted.
_BOARD_RE = re.compile(r"^[A-Za-z0-9_.-]+$")


def run_once(board: str, timeout: int) -> Measurement | str | None:
    if not _BOARD_RE.match(board):
        raise SystemExit(
            f"refusing to run with board name {board!r}: it is "
            "interpolated into a shell command"
        )
    _kill_stale_daemon()
    inner = (
        "source /home/niteris/.clud/tmp/nixcompat/env.sh 2>/dev/null; "
        f"bash autoresearch {board} --mp3 --skip-lint"
    )
    try:
        proc = RunningProcess.run(
            ["sg", "dialout", "-c", f"bash -c '{inner}'"],
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout,
            encoding="utf-8",
            errors="replace",
        )
    except KeyboardInterrupt:  # noqa: KBI002 - guard so the TimeoutExpired
        # handler below cannot swallow Ctrl-C; re-raises unchanged.
        raise
    except subprocess.TimeoutExpired:
        print("  the run itself hung; treating as a transport failure", file=sys.stderr)
        return TRANSPORT_FAILURE
    # Explicit separator: RunningProcess.run strips the trailing newline
    # from each stream, so a bare `+` welds the last stdout line onto the
    # first stderr line -- which here would corrupt the checksum line and
    # report a false "CHECKSUMS DIFFER".
    out = proc.stdout + "\n" + proc.stderr
    return parse_run(out)


def parse_run(out: str) -> Measurement | str | None:
    """Turn one autoresearch transcript into a Measurement.

    Returns TRANSPORT_FAILURE when the serial link, not the decoder, failed;
    None when the run did not produce a usable result."""
    if RE_SUPPRESSED.search(out):
        print(
            "  ratio suppressed: the two decoders did not decode the same "
            "work. Refusing to report a number.",
            file=sys.stderr,
        )
        return None
    if not RE_PASS.search(out):
        tail = "\n".join(out.strip().splitlines()[-6:])
        if RE_TRANSPORT.search(out):
            print("  serial link failed, not the decoder; will retry", file=sys.stderr)
            return TRANSPORT_FAILURE
        print(f"  run did not pass:\n{tail}", file=sys.stderr)
        return None
    l3, dec, fnv = RE_L3.search(out), RE_DECODE.search(out), RE_FNV.search(out)
    if not dec:
        print("  could not parse the result line", file=sys.stderr)
        return None
    if not fnv:
        # The checksum is how a run is shown to have decoded the same audio.
        # A measurement without one is a time with nothing attached to it, and
        # reporting it as a result is how a decoder that quietly broke gets
        # called faster.
        print("  run produced no checksum; refusing to report a time", file=sys.stderr)
        return None
    return Measurement(
        l3_us=int(l3.group(1)) if l3 else None,
        helix_us=int(l3.group(2)) if l3 else None,
        ratio=float(l3.group(3)) if l3 else None,
        decode_us=int(dec.group(1)),
        audio_us=int(dec.group(2)),
        realtime=float(dec.group(3)),
        fnv1a=fnv.group(1),
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--board", default="esp32c6")
    parser.add_argument(
        "--runs", type=int, default=2, help="repeat count; 1 gives no variance estimate"
    )
    parser.add_argument("--timeout", type=int, default=1500)
    parser.add_argument(
        "--retries",
        type=int,
        default=2,
        help="retries per run when the serial link fails",
    )
    parser.add_argument("--json", type=Path, help="write the raw results here")
    args = parser.parse_args(argv)

    results: list[Measurement] = []
    for i in range(args.runs):
        print(f"  run {i + 1}/{args.runs} on {args.board}...", flush=True)
        for attempt in range(args.retries + 1):
            got = run_once(args.board, args.timeout)
            if got is not TRANSPORT_FAILURE:
                break
            if attempt < args.retries:
                # Let the board finish re-enumerating before grabbing it again.
                print(
                    f"    retry {attempt + 1}/{args.retries} "
                    "after settling the board...",
                    flush=True,
                )
                time.sleep(8)
        # `isinstance`, not `got is None or got is TRANSPORT_FAILURE`: the
        # sentinel is a plain `str`, so an identity check cannot narrow `str`
        # out of `Measurement | str | None` and every attribute access below
        # reads as possibly-str. Same three outcomes, in a form the checker
        # can follow.
        if not isinstance(got, Measurement):
            if got is TRANSPORT_FAILURE:
                print(
                    f"  the serial link failed {args.retries + 1} times. "
                    "This is a connection problem, not a measurement.",
                    file=sys.stderr,
                )
            return 1
        results.append(got)
        if got.l3_us is not None:
            print(
                f"    L3 {got.l3_us} us  helix {got.helix_us} us  "
                f"{got.ratio}x  {got.realtime}x real time"
            )
        else:
            print(
                f"    decode {got.decode_us} us  {got.realtime}x real time  "
                f"(no Helix leg)"
            )

    checksums = {r.fnv1a for r in results}
    if len(checksums) > 1:
        # Output changing between runs of the same build is not noise.
        print(f"  CHECKSUMS DIFFER between runs: {checksums}", file=sys.stderr)
        return 1

    # Every run has to be the same *kind* of measurement. A firmware does not
    # grow or lose its Helix leg between runs of one invocation, so runs that
    # disagree mean a truncated transcript -- and averaging a Layer III-only
    # time with a whole-fixture one would move the median by far more than the
    # sub-1% deltas mp3measure exists to resolve.
    legs = {r.l3_us is not None for r in results}
    if len(legs) > 1:
        print(
            "  runs disagree on whether the Helix leg was present; one "
            "transcript was probably truncated. Refusing to mix a Layer "
            "III-only time with a whole-fixture time.",
            file=sys.stderr,
        )
        return 1

    # One series, named for what it actually is. `series` is the Layer III leg
    # when every run has one and the whole-fixture decode otherwise; the label
    # travels with it so the summary can never call a fixture time "L3".
    have_l3 = all(r.l3_us is not None for r in results)
    if have_l3:
        series = [r.l3_us for r in results if r.l3_us is not None]
        label = "minimp3 L3  "
    else:
        series = [r.decode_us for r in results]
        label = "minimp3 fixt"
    helix = [r.helix_us for r in results if r.helix_us is not None]
    ratios = [r.ratio for r in results if r.ratio is not None]
    spread = (max(series) - min(series)) / min(series) * 100 if min(series) else 0.0

    print()
    print(
        f"  {label} median {statistics.median(series):>10,.0f} us   "
        f"spread {spread:.2f}%"
    )
    if helix and ratios:
        print(f"  helix L3     median {statistics.median(helix):>10,.0f} us")
        print(f"  ratio        median {statistics.median(ratios):.3f}x helix")
    else:
        print(
            "  helix        not compiled in; whole-fixture time only "
            "(the ratio needs feat/helix-benchmark-reference)"
        )
    print(f"  checksum     0x{results[0].fnv1a}")
    if args.runs > 1 and spread > 2.0:
        print(
            f"  NOTE: {spread:.1f}% spread across runs -- treat deltas "
            f"smaller than that as unresolved."
        )

    if args.json:
        # asdict, because Measurement became a dataclass and json cannot
        # serialise one directly -- this path is only exercised with --json,
        # so the conversion broke it silently.
        args.json.write_text(json.dumps([asdict(r) for r in results], indent=2))
        print(f"  wrote {args.json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
