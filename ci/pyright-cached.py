#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""
Caching wrapper for pyright that only runs if files have changed.
This significantly speeds up repeated pyright runs during development.
"""

import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional


def get_file_hash(file_path: Path) -> str:
    """Get SHA256 hash of a file."""
    try:
        with open(file_path, "rb") as f:
            return hashlib.sha256(f.read()).hexdigest()
    except (OSError, IOError):
        return ""


def get_directory_hash(directory: Path, extensions: Optional[List[str]] = None) -> str:
    """Get combined hash of all files in a directory."""
    if extensions is None:
        extensions = [".py"]

    hashes: List[str] = []
    if directory.exists():
        for ext in extensions:
            for file_path in directory.rglob(f"*{ext}"):
                if file_path.is_file():
                    hashes.append(
                        f"{file_path.relative_to(directory)}:{get_file_hash(file_path)}"
                    )

    hashes.sort()  # Ensure consistent ordering
    return hashlib.sha256("\n".join(hashes).encode()).hexdigest()


def get_cache_file() -> Path:
    """Get the cache file path."""
    cache_dir = Path(".cache/pyright")
    cache_dir.mkdir(parents=True, exist_ok=True)
    return cache_dir / "file_hashes.json"


def load_cache() -> Dict[str, str]:
    """Load the cache file."""
    cache_file = get_cache_file()
    if cache_file.exists():
        try:
            with open(cache_file, "r") as f:
                return json.load(f)
        except (json.JSONDecodeError, IOError):
            pass
    return {}


def save_cache(cache: Dict[str, str]) -> None:
    """Save the cache file."""
    cache_file = get_cache_file()
    try:
        with open(cache_file, "w") as f:
            json.dump(cache, f, indent=2)
    except IOError:
        pass  # Ignore cache save errors


def has_files_changed() -> bool:
    """Check if any relevant files have changed since last run."""
    cache = load_cache()
    changed = False

    # Files and directories to check
    files_to_check = [
        Path("pyrightconfig.json"),
        Path(".pyrightconfig.json"),
        Path("pyproject.toml"),
    ]

    directories_to_check = [
        Path("ci"),
    ]

    # Check individual files
    for file_path in files_to_check:
        if file_path.exists():
            current_hash = get_file_hash(file_path)
            if cache.get(str(file_path)) != current_hash:
                cache[str(file_path)] = current_hash
                changed = True

    # Check directories
    for dir_path in directories_to_check:
        if dir_path.exists():
            current_hash = get_directory_hash(dir_path)
            cache_key = f"dir:{dir_path}"
            if cache.get(cache_key) != current_hash:
                cache[cache_key] = current_hash
                changed = True

    # Save cache if any changes were detected
    if changed:
        save_cache(cache)

    return changed


def run_pyright(args: List[str]) -> int:
    """Run pyright with the given arguments."""
    env = os.environ.copy()
    # env["PYRIGHT_PYTHON_FORCE_VERSION"] = "latest"
    env["PYRIGHT_PYTHON_CACHE_DIR"] = ".cache/pyright"

    cmd = ["uv", "run", "pyright"] + args

    try:
        result = subprocess.run(cmd, env=env)
        return result.returncode
    except KeyboardInterrupt:
        return 1
    except Exception as e:
        print(f"Error running pyright: {e}", file=sys.stderr)
        return 1


def main() -> int:
    """Main entry point."""
    args = sys.argv[1:]  # Remove script name

    # If --force is passed, skip cache check
    if "--force" in args:
        args.remove("--force")
        print("Forcing pyright run (--force flag detected)")
        return run_pyright(args)

        # Check if files have changed
    if not has_files_changed():
        print("No changes detected, skipping pyright (use --force to override)")
        return 0

    print("Changes detected, running pyright...")
    return run_pyright(args)


if __name__ == "__main__":
    sys.exit(main())
