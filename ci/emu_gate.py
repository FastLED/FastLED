#!/usr/bin/env python3
"""Select or run declared emulator gates (Python 3.11+, stdlib only).

Usage: uv run --no-project ci/emu_gate.py --base origin/master --head HEAD
Add --run to execute, or --json for the v1 attestation payload. Exit codes:
0 = selected/all executed gates passed; 3 = no gates; 1 = execution/config error.
Unmapped paths are always reported, including declared no-emulator gaps.

The worktree's ci.toml [allow].platform-code is a list of records with paths
(glob strings), gates ("emulator:environment" strings), and optional gap text.
Overlapping records union their gates; an empty gates list is a coverage gap.
The declaration is supplied by the platform-code policy work item separately.

--run expects projects already staged at .build/fbuild/<environment>, as in
ci/stage_fbuild_project.py. It never stages, installs, or downloads tools itself;
fbuild owns building/emulation and must already be on PATH. All child output and
per-test duration lines go to stderr, leaving --json stdout as one object.
"""

import argparse
import asyncio
import json
import math
import os
import re
import signal
import sys
import time
from dataclasses import asdict, dataclass
from fnmatch import fnmatchcase
from functools import lru_cache
from pathlib import Path

import tomllib


@dataclass
class Mapping:
    paths: list[str]
    gates: list[str]
    gap: str


@dataclass
class Unmapped:
    path: str
    platform: str
    reason: str


@dataclass
class Result:
    gate: str
    status: str
    returncode: int | None
    duration_s: float


def read_mapping(repo: Path) -> list[Mapping]:
    """Read the platform -> emulator-test map.

    It lives in `ci/emu_map.toml`, not in `[allow].platform-code`: ci-lint's
    schema types that field as a flat list of path globs (a classification of
    what counts as platform code), so a record shape there would fail precheck.
    """
    source_path = repo / "ci" / "emu_map.toml"
    if not source_path.is_file():
        raise ValueError("ci/emu_map.toml is missing")
    with source_path.open("rb") as source:
        entries = tomllib.load(source).get("platforms")
    if not isinstance(entries, list):
        raise ValueError("ci/emu_map.toml must declare a [[platforms]] array")
    mappings: list[Mapping] = []
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError(
                "platform-code entries must be {paths, gates, gap?} records"
            )
        paths, gates, gap = (
            entry.get("paths"),
            entry.get("gates"),
            entry.get("gap", "no emulator"),
        )
        if (
            not isinstance(paths, list)
            or not paths
            or not all(isinstance(p, str) and p for p in paths)
        ):
            raise ValueError("platform-code.paths must be a nonempty list of globs")
        if not isinstance(gates, list) or not all(
            isinstance(g, str)
            and re.fullmatch(r"[\w-]+:[\w.-]+", g, flags=re.ASCII)
            and g.split(":")[1] not in (".", "..")
            for g in gates
        ):
            raise ValueError(
                "platform-code.gates must contain emulator:environment identifiers"
            )
        if not isinstance(gap, str):
            raise ValueError("platform-code.gap must be text")
        mappings.append(Mapping(paths, gates, gap))
    return mappings


def matches_path(path: str, pattern: str) -> bool:
    """Repository globs: * stays in one component; ** spans components."""
    parts, globs = path.split("/"), pattern.split("/")

    @lru_cache(maxsize=None)
    def match(i: int, j: int) -> bool:
        if j == len(globs):
            return i == len(parts)
        if globs[j] == "**":
            return match(i, j + 1) or (i < len(parts) and match(i + 1, j))
        return (
            i < len(parts) and fnmatchcase(parts[i], globs[j]) and match(i + 1, j + 1)
        )

    return match(0, 0)


PLATFORM_ROOT = "src/platforms/"


def select(
    changed: list[str], mappings: list[Mapping]
) -> tuple[list[str], list[Unmapped]]:
    required: set[str] = set()
    unmapped: list[Unmapped] = []
    for path in changed:
        matches = [m for m in mappings if any(matches_path(path, p) for p in m.paths)]
        if not matches and not path.startswith(PLATFORM_ROOT):
            # Not platform code, so this gate has no opinion. Reporting it would
            # bury the real gaps (a platform path nobody mapped) in noise, and
            # noise is what teaches people to ignore the output.
            continue
        required.update(g for m in matches for g in m.gates)
        gaps = [m.gap for m in matches if not m.gates]
        if not matches or gaps:
            parts = path.split("/")
            platform = (
                parts[2] if len(parts) > 2 and parts[:2] == ["src", "platforms"] else ""
            )
            unmapped.append(
                Unmapped(path, platform, "; ".join(dict.fromkeys(gaps)) or "no mapping")
            )
    return sorted(required), unmapped


async def execute(
    args: list[str], repo: Path, timeout: float, capture: bool = False
) -> tuple[int, bytes]:
    """Drain git output; stream tests to stderr. Kill the process group on timeout/cancel."""
    child = await asyncio.create_subprocess_exec(
        *args,
        cwd=repo,
        start_new_session=os.name == "posix",
        stdout=asyncio.subprocess.PIPE if capture else sys.stderr,
        stderr=asyncio.subprocess.PIPE if capture else sys.stderr,
    )
    try:
        stdout, stderr = await asyncio.wait_for(child.communicate(), timeout)
    except (TimeoutError, asyncio.CancelledError):
        if os.name == "posix":
            try:
                os.killpg(child.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        elif child.returncode is None:
            child.kill()
        await child.wait()
        raise
    if capture and child.returncode:
        raise ValueError(
            (stderr or b"command failed").decode("utf-8", errors="replace").strip()
        )
    return child.returncode or 0, stdout or b""


async def changed_paths(repo: Path, base: str, head: str) -> list[str]:
    refs: list[str] = []
    for ref in (base, head):
        _, output = await execute(
            ["git", "rev-parse", "--verify", "--end-of-options", ref + "^{commit}"],
            repo,
            10,
            True,
        )
        refs.append(output.decode().strip())
    _, output = await execute(
        [
            "git",
            "diff",
            "--no-ext-diff",
            "--no-renames",
            "--name-only",
            "-z",
            *refs,
            "--",
        ],
        repo,
        10,
        True,
    )
    return sorted({os.fsdecode(path) for path in output.split(b"\0") if path})


async def run_gates(
    repo: Path, gates: list[str], timeout: float, budget: float
) -> list[Result]:
    deadline = time.monotonic() + budget
    results: list[Result] = []
    for gate in gates:
        start = time.monotonic()
        remaining = deadline - start
        status, returncode = "budget_exhausted", None
        if remaining > 0:
            emulator, env = gate.split(":")
            try:
                returncode, _ = await execute(
                    [
                        "fbuild",
                        "test-emu",
                        "--emulator",
                        emulator,
                        "-e",
                        env,
                        "--timeout",
                        str(min(120, math.ceil(timeout))),
                        str(repo / ".build" / "fbuild" / env),
                    ],
                    repo,
                    min(timeout, remaining),
                )
                status = "passed" if returncode == 0 else "failed"
            except TimeoutError:
                status = "timeout"
            except OSError as error:
                status = "error"
                print(f"{gate}: {error}", file=sys.stderr)
        result = Result(gate, status, returncode, round(time.monotonic() - start, 3))
        results.append(result)
        print(f"{gate} {status} duration={result.duration_s:.3f}s", file=sys.stderr)
    return results


def bounded_seconds(value: str) -> float:
    seconds = float(value)
    if not math.isfinite(seconds) or not 0 < seconds <= 600:
        raise argparse.ArgumentTypeError(
            "must be greater than 0 and at most 600 seconds"
        )
    return seconds


async def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--repo", type=Path, default=Path(__file__).resolve().parents[1]
    )
    parser.add_argument("--base", default="origin/master")
    parser.add_argument("--head", default="HEAD")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="select only (default)")
    mode.add_argument("--run", action="store_true", help="execute selected gates")
    parser.add_argument(
        "--json", action="store_true", help="emit one v1 object on stdout"
    )
    parser.add_argument(
        "--timeout",
        type=bounded_seconds,
        default=300,
        help="per-test wall timeout (default: 300s)",
    )
    parser.add_argument(
        "--budget",
        type=bounded_seconds,
        default=600,
        help="total run budget, at most 600s",
    )
    args = parser.parse_args(argv)
    repo = args.repo.resolve()
    try:
        mappings = read_mapping(repo)
        changed = await changed_paths(repo, args.base, args.head)
        required, unmapped = select(changed, mappings)
        for item in unmapped:
            detail = f" ({item.platform}: {item.reason})" if item.platform else ""
            print(f"unmapped {item.path}{detail}", file=sys.stderr)
        results = (
            await run_gates(repo, required, args.timeout, args.budget)
            if args.run
            else []
        )
    except (OSError, ValueError, TimeoutError) as error:
        print(f"emu_gate: {error}", file=sys.stderr)
        return 1
    if args.json:
        print(
            json.dumps(
                {
                    "v": 1,
                    "changed": changed,
                    "required": required,
                    "unmapped": [asdict(item) for item in unmapped],
                    "results": [asdict(result) for result in results],
                }
            )
        )
    else:
        for gate in required:
            print(gate)
    if any(result.status != "passed" for result in results):
        return 1
    return 0 if required else 3


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
