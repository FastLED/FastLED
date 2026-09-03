#!/usr/bin/env python3
"""One command: measure an MP3 decoder change on host and hardware.

    uv run python ci/codec_cpu/report.py --baseline HEAD

Prints host instruction counts, the device ratio with a variance estimate, the
riscv32 .text delta and the accuracy figure -- everything needed to answer "did
my edit help" -- in roughly twenty-five lines.

Both halves are needed and neither is sufficient. On this decoder the host and
the ESP32-C6 have disagreed in both direction and magnitude: a change measured
at -4.8% on host delivered nothing on device, and a polyphase restructure that
read as a +3.2% *regression* on host delivered -9.7% on the C6. Quote the device
ratio for speed claims.

Subprocess output is captured to files rather than printed. An earlier version
let the lint gate emit a hundred lines of cargo warnings plus a help screen into
the middle of the report, which meant re-running the whole thing -- including a
58-second device flash -- through grep just to read thirty lines.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from running_process import RunningProcess


ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent

PSNR_PROBE = """
import sys, importlib.util
spec = importlib.util.spec_from_file_location(
    'cr', 'ci/codec_conformance/run.py')
m = importlib.util.module_from_spec(spec); sys.modules['cr'] = m
spec.loader.exec_module(m)
v = m.fetch_vectors(); b = m.build_harness()
for bit, ref in m.collect(v):
    o = m.run_vector(b, bit, ref)
    if o.name == 'l3-hecommon':
        print(f'PSNR={o.psnr:.2f}')
"""

# 123.24 dB is what the decoder holds today and 20.4 dB better than the
# reference it replaced. It is not currency; a change that spends it needs to
# say so explicitly rather than let it slide past in a wall of output.
PSNR_EXPECTED = 123.24


def _run(command: list[str], label: str, logs: Path, quiet: bool = True):
    """Run a step, print its verdict, keep its output in a file."""
    started = time.monotonic()
    proc = RunningProcess.run(
        command,
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    text = proc.stdout + proc.stderr
    log = logs / (re.sub(r"[^a-z0-9]+", "-", label.lower()).strip("-") + ".log")
    log.write_text(text)
    if not quiet:
        for line in text.splitlines():
            if "libxml2" not in line and "no version information" not in line:
                print(line)
    mark = "ok " if proc.returncode == 0 else "FAIL"
    print(f"  [{mark}] {label:<34} {time.monotonic() - started:5.0f}s  {log}")
    return proc.returncode, text


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--baseline",
        default="HEAD",
        help="git ref to compare against; HEAD measures uncommitted work",
    )
    parser.add_argument("--board", default="esp32c6")
    parser.add_argument(
        "--runs",
        type=int,
        default=2,
        help="device repeats; 1 gives no variance estimate",
    )
    parser.add_argument("--skip-host", action="store_true")
    parser.add_argument("--skip-device", action="store_true")
    parser.add_argument(
        "--skip-accuracy",
        action="store_true",
        help="do not check PSNR; you almost never want this",
    )
    parser.add_argument(
        "--gates",
        action="store_true",
        help="also run full conformance, sanitizers and lint",
    )
    parser.add_argument(
        "--keep-logs", type=Path, help="write step logs here instead of a temp dir"
    )
    args = parser.parse_args(argv)

    logs = args.keep_logs or Path(tempfile.mkdtemp(prefix="mp3report-"))
    logs.mkdir(parents=True, exist_ok=True)
    failures: list[str] = []

    if not args.skip_host:
        print("\nHOST -- Callgrind, scalar, -Os, deterministic")
        rc, _ = _run(
            [sys.executable, str(HERE / "callgrind.py"), "--baseline", args.baseline],
            "host profile",
            logs,
            quiet=False,
        )
        if rc:
            failures.append("host profile")

        print("\nSIZE -- riscv32 -Os .text")
        rc, _ = _run(
            [sys.executable, str(HERE / "text_size.py"), "--baseline", args.baseline],
            "text size",
            logs,
            quiet=False,
        )
        if rc:
            failures.append("text size")

    if not args.skip_device:
        print(f"\nDEVICE -- {args.board}, {args.runs} run(s), the authority")
        rc, _ = _run(
            [
                sys.executable,
                str(HERE / "device_profile.py"),
                "--board",
                args.board,
                "--runs",
                str(args.runs),
            ],
            "device profile",
            logs,
            quiet=False,
        )
        if rc:
            failures.append("device profile")

    if not args.skip_accuracy:
        # Accuracy is checked every run, not behind --gates. A sub-agent
        # optimizing fixed-point math can "win" by breaking the math, and an
        # opt-in tripwire is one the loop will skip.
        print("\nACCURACY -- the tripwire, checked every run")
        rc, text = _run([sys.executable, "-c", PSNR_PROBE], "PSNR", logs)
        match = re.search(r"PSNR=([\d.]+)", text)
        if rc or not match:
            failures.append("PSNR did not run")
        else:
            psnr = float(match.group(1))
            drift = psnr - PSNR_EXPECTED
            if abs(drift) < 0.005:
                print(f"  [ok ] l3-hecommon {psnr:.2f} dB, unchanged")
            else:
                print(
                    f"  [FAIL] l3-hecommon {psnr:.2f} dB, "
                    f"moved {drift:+.2f} dB from {PSNR_EXPECTED:.2f}"
                )
                failures.append(f"PSNR moved {drift:+.2f} dB")

    if args.gates:
        print("\nGATES")
        conformance = ROOT / "ci" / "codec_conformance" / "run.py"

        for label, command in (
            ("conformance, fixed", [sys.executable, str(conformance)]),
            ("conformance, float", [sys.executable, str(conformance), "--float"]),
            ("sanitizers", [sys.executable, str(conformance), "--sanitize"]),
            ("lint", ["bash", "lint", "--cpp"]),
        ):
            rc, _ = _run(command, label, logs)
            if rc:
                failures.append(label)

    print()
    if failures:
        print(f"FAILED: {', '.join(failures)}")
        print(f"Logs in {logs}")
        return 1
    print(f"All measurements completed. Logs in {logs}")
    print("Quote the DEVICE ratio for speed claims: host and device have")
    print("disagreed by 2.5x on this decoder, and static instruction counts")
    print("bound a win rather than measuring it.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
