#!/usr/bin/env python3
"""
Caching wrapper for black that only runs if Python files have changed.
This significantly speeds up repeated black runs during development.
"""

import hashlib
import json
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

    hashes = []
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
    cache_dir = Path(".cache/black")
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


def has_files_changed(directories: List[Path]) -> bool:
    """Check if any relevant files have changed since last run."""
    cache = load_cache()
    changed = False

    # Files to check
    files_to_check = [
        Path("pyproject.toml"),
        Path(".black"),
    ]

    # Check individual files
    for file_path in files_to_check:
        if file_path.exists():
            current_hash = get_file_hash(file_path)
            if cache.get(str(file_path)) != current_hash:
                cache[str(file_path)] = current_hash
                changed = True

    # Check directories
    for dir_path in directories:
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


def run_black(args: List[str]) -> int:
    """Run black with the given arguments."""
    cmd = ["uv", "run", "black"] + args

    try:
        result = subprocess.run(cmd)
        return result.returncode
    except KeyboardInterrupt:
        return 1
    except Exception as e:
        print(f"Error running black: {e}", file=sys.stderr)
        return 1


def main() -> int:
    """Main entry point."""
    args = sys.argv[1:]  # Remove script name

    # If --force is passed, skip cache check
    if "--force" in args:
        args.remove("--force")
        print("Forcing black run (--force flag detected)")
        return run_black(args)

    # Extract directories to check from arguments
    directories = []
    for arg in args:
        if not arg.startswith("-"):
            path = Path(arg)
            if path.exists() and path.is_dir():
                directories.append(path)
            elif path.exists() and path.is_file():
                directories.append(path.parent)

    # If no directories specified, assume current directory
    if not directories:
        directories = [Path(".")]

        # Check if files have changed
    if not has_files_changed(directories):
        print("No changes detected, skipping black (use --force to override)")
        return 0

    print("Changes detected, running black...")
    return run_black(args)


if __name__ == "__main__":
    sys.exit(main())
