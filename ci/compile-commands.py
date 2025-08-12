#!/usr/bin/env python
"""
Compile Commands Generator

Generates a compile_commands.json for clangd/clang-tidy based on explicit
tools/flags configuration. See FEATURE.md for usage and rationale.

Key behaviors:
- Reads configuration from ci/build_commands.toml
- Enumerates library source files under src/ (respecting optional filters.exclude_dirs)
- Emits one entry per translation unit with accurate arguments/command fields

Notes:
- This script intentionally avoids project-unit-test driven flags to keep
  IntelliSense consistent with the core FastLED library build.
"""

from __future__ import annotations

import _thread
import argparse
import json
import os
import subprocess
import sys

# Require Python 3.11+ environment for TOML parsing
import tomllib
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional, cast

# Require typeguard; no try-import fallbacks
from typeguard import typechecked

# Use the canonical BuildFlags/Compiler tooling
from ci.compiler.clang_compiler import commands_json


PROJECT_ROOT: Path = Path(__file__).resolve().parent.parent
# Default to bespoke config for compile-commands generation
DEFAULT_CONFIG_PATH: Path = PROJECT_ROOT / "ci" / "build_commands.toml"


@typechecked
@dataclass
class CliConfig:
    build_flags_toml: Path
    exclude_dirs: list[str]


@typechecked
@dataclass
class Args:
    output: Path
    clean: bool
    verbose: bool
    config: Path
    exclude: list[str]


def parse_args(args: Optional[list[str]] = None) -> Args:
    parser = argparse.ArgumentParser(
        description=(
            "Generate compile_commands.json for clangd/clang-tidy based on explicit build configuration."
        )
    )
    parser.add_argument(
        "--output",
        dest="output",
        metavar="PATH",
        help="Output path for compile_commands.json (default: project root)",
    )
    parser.add_argument(
        "--config",
        dest="config",
        metavar="PATH",
        help=(
            "Path to BuildFlags-style TOML (default: ci/build_example.toml). "
            "Must be parseable by BuildFlags.parse()."
        ),
    )
    parser.add_argument(
        "--clean",
        dest="clean",
        action="store_true",
        help="Rebuild command set from scratch (ignored currently; reserved)",
    )
    parser.add_argument(
        "--verbose",
        dest="verbose",
        action="store_true",
        help="Print collected flags/files",
    )
    parser.add_argument(
        "--exclude-dir",
        dest="exclude",
        action="append",
        default=[],
        help="Relative directory under project root to exclude (can be used multiple times)",
    )

    parsed = parser.parse_args(args)

    output_path_str: Optional[str] = None
    if hasattr(parsed, "output"):
        output_path_str = parsed.output

    if output_path_str is None or len(output_path_str) == 0:
        # Default to project root compile_commands.json
        output_path = PROJECT_ROOT / "compile_commands.json"
    else:
        output_path = Path(output_path_str).resolve()

    clean_value: bool = False
    if hasattr(parsed, "clean"):
        clean_value = bool(parsed.clean)

    verbose_value: bool = False
    if hasattr(parsed, "verbose"):
        verbose_value = bool(parsed.verbose)

    # Config path
    config_path: Path
    if getattr(parsed, "config", None):
        config_path = Path(parsed.config).resolve()
    else:
        config_path = DEFAULT_CONFIG_PATH

    # Excluded directories
    exclude_list: list[str] = list(getattr(parsed, "exclude", []) or [])

    return Args(
        output=output_path,
        clean=clean_value,
        verbose=verbose_value,
        config=config_path,
        exclude=exclude_list,
    )


def _assert_config_exists(config_path: Path) -> None:
    if not config_path.exists():
        raise RuntimeError(
            f"CRITICAL: Required BuildFlags TOML not found at {config_path}. "
            f"Please provide a valid configuration parseable by BuildFlags.parse()."
        )


def _create_cli_config(build_flags_toml: Path, exclude: list[str]) -> CliConfig:
    return CliConfig(build_flags_toml=build_flags_toml, exclude_dirs=exclude)


def _collect_source_files(root: Path, exclude: list[str], verbose: bool) -> list[Path]:
    source_files: list[Path] = []

    # Compute excluded directories as absolute paths for quick prefix checks
    excluded_dirs_abs: list[Path] = []
    for ex in exclude:
        p = (PROJECT_ROOT / ex).resolve()
        excluded_dirs_abs.append(p)

    src_root: Path = PROJECT_ROOT / "src"
    if not src_root.exists():
        raise RuntimeError(
            f"CRITICAL: Expected source root at {src_root} not found. "
            f"This path is required to enumerate translation units."
        )

    for dirpath, dirnames, filenames in os.walk(src_root):
        current_dir = Path(dirpath).resolve()

        # Apply directory exclusions
        skip_dir: bool = False
        for ex in excluded_dirs_abs:
            try:
                current_dir.relative_to(ex)
                skip_dir = True
                break
            except ValueError:
                # Not under this excluded path
                pass

        if skip_dir:
            # Prune search by clearing dirnames to avoid descending further
            del dirnames[:]
            continue

        for fname in filenames:
            if fname.endswith(".cpp") or fname.endswith(".c"):
                source_files.append(current_dir / fname)

    if verbose:
        print(f"Discovered {len(source_files)} source files under {src_root}")

    if len(source_files) == 0:
        raise RuntimeError(
            "CRITICAL: No source files discovered under 'src/'. "
            "Please verify the repository layout and filters.exclude_dirs configuration."
        )

    return source_files

    # No per-file argument composition here; delegated to commands_json()
    # This function intentionally removed to rely on canonical toolchain

    # Emission handled by commands_json()


def main(entry_args: Optional[list[str]] = None) -> int:
    args = parse_args(entry_args)

    # Log startup context
    print("[compile-commands] Starting generation")
    print(f"[compile-commands] Project root: {PROJECT_ROOT}")
    print(f"[compile-commands] Config (BuildFlags TOML): {args.config}")
    print(f"[compile-commands] Output path: {args.output}")
    if args.exclude:
        print(f"[compile-commands] Excluding directories: {args.exclude}")
    else:
        print("[compile-commands] No excluded directories configured")

    # Determine BuildFlags TOML path
    _assert_config_exists(args.config)

    # Collect source files
    source_root = PROJECT_ROOT / "src"
    print(f"[compile-commands] Source root: {source_root}")
    sources = _collect_source_files(
        source_root,
        exclude=args.exclude,
        verbose=args.verbose,
    )
    print(f"[compile-commands] Discovered {len(sources)} source files")

    # Delegate to canonical commands_json() using BuildFlags toolchain
    print("[compile-commands] Generating compile_commands.json entries...")
    commands_json(
        config_path=args.config,
        include_root=source_root,
        sources=sources,
        output_json=args.output,
        quick_build=True,
        strict_mode=False,
    )

    # Post-generation verification
    if not args.output.exists():
        print(
            f"CRITICAL: Generation completed but output file was not found at {args.output}"
        )
        return 2

    try:
        text = args.output.read_text(encoding="utf-8")
        # Best-effort count of entries to report status
        import json as _json
        from typing import cast

        entries = cast(list[dict[str, Any]], _json.loads(text))
        num_entries = len(entries) if isinstance(entries, list) else 0
        print(
            f"[compile-commands] Wrote {num_entries} entries to {args.output} (size: {args.output.stat().st_size} bytes)"
        )
    except Exception as e:  # noqa: BLE001
        print(
            f"WARNING: Output written to {args.output} but could not parse JSON for summary: {e}"
        )

    if args.verbose:
        print("[compile-commands] Done.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("Operation interrupted by user")
        _thread.interrupt_main()
        raise
    except FileNotFoundError as e:
        print(f"CRITICAL: Required file not found: {e}")
        sys.exit(1)
    except ValueError as e:
        print(f"CRITICAL: Invalid configuration value: {e}")
        sys.exit(1)
    except Exception as e:  # noqa: BLE001
        print(f"CRITICAL: Unexpected error: {e}")
        raise
