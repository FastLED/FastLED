"""FastLED reciprocal-license source-header compliance stage."""

import argparse
import hashlib
import importlib.util
import sys
from pathlib import Path
from typing import Any, cast

from running_process import RunningProcess


def load_header_tool(repo_root: Path) -> Any:
    name = "_fastled_vendored_license_headers"
    spec = importlib.util.spec_from_file_location(
        name, repo_root / "tools/license_headers.py"
    )
    if spec is None or spec.loader is None:
        raise RuntimeError("unable to load vendored license header tool")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return cast(Any, module)


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


def manifest_paths(repo_root: Path, manifest_name: str) -> set[str] | None:
    try:
        lines = (repo_root / manifest_name).read_text(encoding="utf-8").splitlines()
        paths = {line.split("  ", 1)[1] for line in lines}
    except (OSError, IndexError):
        return None
    return paths if len(paths) == len(lines) else None


def verify_path_coverage(manifest: set[str], inventory: set[str]) -> bool:
    missing = sorted(inventory - manifest)
    extra = sorted(manifest - inventory)
    for relative in missing:
        print(f"excluded source missing from hash manifest: {relative}")
    for relative in extra:
        print(f"hash manifest path is not excluded source: {relative}")
    return not missing and not extra


def verify_exclusion_coverage(repo_root: Path) -> bool:
    manifest = manifest_paths(repo_root, "EXCLUDED-SOURCE.sha256")
    if manifest is None:
        print("excluded source manifest has malformed or duplicate paths")
        return False
    header_tool = load_header_tool(repo_root)
    policy = header_tool.load_policy(repo_root / "header-policy.toml", "release")
    rg = header_tool.resolve_ripgrep(repo_root)
    inventory = {
        finding.relative
        for finding in header_tool.inventory(policy, rg)
        if finding.state is header_tool.State.EXCLUDED
    }
    return verify_path_coverage(manifest, inventory)


def run(*, no_cache: bool = False) -> bool:
    repo_root = Path(__file__).parents[2]
    for manifest_name in ("LICENSE-ARTIFACTS.sha256", "EXCLUDED-SOURCE.sha256"):
        if not verify_manifest(repo_root, manifest_name):
            return False
    if not verify_exclusion_coverage(repo_root):
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
