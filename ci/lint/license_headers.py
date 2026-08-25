"""FastLED reciprocal-license source-header compliance stage."""

import argparse
import hashlib
from pathlib import Path

from running_process import RunningProcess


def verify_manifest(repo_root: Path, manifest_name: str) -> bool:
    manifest = repo_root / manifest_name
    try:
        lines = manifest.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        print(f"license artifact manifest unreadable: {manifest_name}: {error}")
        return False
    for line in lines:
        try:
            expected, relative = line.split("  ", 1)
        except ValueError:
            print(f"malformed license artifact manifest line: {manifest_name}: {line}")
            return False
        candidate = (repo_root / relative).resolve()
        if not candidate.is_relative_to(repo_root) or not candidate.is_file():
            print(f"unsafe or missing license artifact: {relative}")
            return False
        actual = hashlib.sha256(candidate.read_bytes()).hexdigest()
        if actual != expected:
            print(f"license artifact hash mismatch: {relative}")
            return False
    return True


def run(*, no_cache: bool = False) -> bool:
    repo_root = Path(__file__).parents[2]
    for manifest_name in ("LICENSE-ARTIFACTS.sha256", "EXCLUDED-SOURCE.sha256"):
        if not verify_manifest(repo_root, manifest_name):
            return False
    command = [
        "uv",
        "run",
        "tools/license_headers.py",
        "check",
        "--profile",
        "release",
    ]
    if no_cache:
        command.append("--no-cache")
    result = RunningProcess.run(
        command,
        cwd=str(repo_root),
        check=False,
        capture_output=True,
        text=True,
        timeout=180,
    )
    if result.stdout:
        print(result.stdout, end="" if result.stdout.endswith("\n") else "\n")
    if result.stderr:
        print(result.stderr, end="" if result.stderr.endswith("\n") else "\n")
    return result.returncode == 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-cache", action="store_true")
    options = parser.parse_args()
    raise SystemExit(0 if run(no_cache=options.no_cache) else 1)
