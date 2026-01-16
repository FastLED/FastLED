"""Argument parsing and configuration for FastLED compilation."""

import argparse
import sys
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Optional

from ci.boards import Board, create_board
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


class WorkflowType(Enum):
    """Compilation workflow types."""

    NATIVE = "native"
    DOCKER = "docker"
    WASM = "wasm"


@dataclass(frozen=True)
class CompilationConfig:
    """Immutable compilation configuration with validation."""

    # Core configuration
    boards: list[Board]
    examples: list[str]
    workflow: WorkflowType

    # Build options
    defines: list[str] = field(default_factory=lambda: [])
    extra_packages: list[str] = field(default_factory=lambda: [])
    verbose: bool = False
    enable_cache: bool = False

    # Output options
    output_path: Optional[Path] = None
    merged_bin: bool = False
    log_failures: Optional[Path] = None
    max_failures: Optional[int] = None

    # Docker options
    docker_build: bool = False
    force_local: bool = False  # True when --local explicitly specified

    # WASM options
    wasm_run: bool = False

    # Advanced options
    global_cache_dir: Optional[Path] = None
    skip_filters: bool = (
        False  # True to skip @filter directives (override platform/memory constraints)
    )
    no_parallel: bool = False  # Disable parallel compilation (currently a no-op)

    def validate(self) -> list[str]:
        """Validate configuration and return list of error messages."""
        errors: list[str] = []

        # Validate merged-bin requirements
        if self.merged_bin:
            if len(self.examples) != 1:
                errors.append(
                    f"--merged-bin requires exactly one sketch, got {len(self.examples)}"
                )
            if len(self.boards) != 1:
                errors.append(
                    f"--merged-bin requires exactly one board, got {len(self.boards)}"
                )
            if self.boards and not self.boards[0].board_name.startswith("esp"):
                errors.append(
                    f"--merged-bin only supports ESP32/ESP8266, got {self.boards[0].board_name}"
                )

        # Validate output path requirements
        if self.output_path:
            if len(self.examples) != 1:
                errors.append(
                    f"-o/--out requires exactly one sketch, got {len(self.examples)}"
                )
            if len(self.boards) != 1:
                errors.append(
                    f"-o/--out requires exactly one board, got {len(self.boards)}"
                )

        # Validate WASM requirements
        if self.workflow == WorkflowType.WASM:
            if len(self.examples) != 1:
                errors.append(
                    f"WASM compilation requires exactly one example, got {len(self.examples)}"
                )
            if len(self.boards) != 1 or self.boards[0].board_name != "wasm":
                errors.append("WASM compilation requires board='wasm'")

        # Validate max_failures
        if self.max_failures is not None and self.max_failures <= 0:
            errors.append(
                f"--max-failures must be a positive integer, got {self.max_failures}"
            )

        return errors


class CompilationArgumentParser:
    """Parses and validates compilation arguments."""

    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.examples_dir = project_root / "examples"

    def parse(self, args: Optional[list[str]] = None) -> CompilationConfig:
        """Parse arguments and return validated config."""
        parser = self._create_parser()

        # Parse arguments with intelligent unknown handling
        try:
            parsed = parser.parse_intermixed_args(args)
            unknown: list[str] = []
        except SystemExit:
            parsed, unknown = parser.parse_known_args(args)

        # Add unknown non-flag arguments as examples
        if unknown:
            extra_examples: list[str] = [
                arg for arg in unknown if not arg.startswith("-")
            ]
            if extra_examples:
                if (
                    not hasattr(parsed, "positional_examples")
                    or parsed.positional_examples is None
                ):
                    parsed.positional_examples = []
                # Type ignore for argparse.Namespace attribute which is dynamically set
                parsed.positional_examples.extend(extra_examples)  # type: ignore[union-attr]

        # Build configuration
        config = self._build_config(parsed)

        # Validate
        errors = config.validate()
        if errors:
            print("❌ Configuration validation failed:")
            for error in errors:
                print(f"  - {error}")
            sys.exit(1)

        return config

    def _create_parser(self) -> argparse.ArgumentParser:
        """Create argument parser with all options."""
        parser = argparse.ArgumentParser(
            description="Compile FastLED examples for various boards using PioCompiler"
        )

        # Positional arguments
        parser.add_argument(
            "boards",
            type=str,
            help="Comma-separated list of boards to compile for",
            nargs="?",
        )
        parser.add_argument(
            "positional_examples",
            type=str,
            help="Examples to compile (positional arguments after board name)",
            nargs="*",
        )

        # Build options
        parser.add_argument(
            "--examples", type=str, help="Comma-separated list of examples to compile"
        )
        parser.add_argument(
            "--exclude-examples", type=str, help="Examples that should be excluded"
        )
        parser.add_argument(
            "--no-filter",
            action="store_true",
            help="Disable @filter directives (compile even if incompatible with board). "
            "By default, filters always apply to prevent compilation failures.",
        )
        parser.add_argument(
            "--defines", type=str, help="Comma-separated list of compiler definitions"
        )
        parser.add_argument(
            "--extra-packages",
            type=str,
            help="Comma-separated list of extra PlatformIO library packages to install (e.g., 'OctoWS2811')",
        )
        parser.add_argument(
            "-v", "--verbose", action="store_true", help="Enable verbose output"
        )
        parser.add_argument(
            "--enable-cache",
            action="store_true",
            help="Enable sccache for faster compilation",
        )
        parser.add_argument(
            "--cache",
            action="store_true",
            help="(Deprecated) Enable sccache for faster compilation - use --enable-cache instead",
        )
        parser.add_argument(
            "--no-cache",
            action="store_true",
            help="Disable sccache for faster compilation (default is already disabled)",
        )

        # Output options
        parser.add_argument(
            "-o",
            "--out",
            type=str,
            help="Output path for build artifact. Requires exactly one sketch. "
            "If path ends with '/', treated as directory. If has suffix, treated as file. "
            "Use '-o .' to save in current directory with sketch name.",
        )
        parser.add_argument(
            "--merged-bin",
            action="store_true",
            help="Generate merged binary for QEMU/flashing (ESP32/ESP8266 only). "
            "Produces a single flash image instead of separate bootloader/firmware/partition files.",
        )
        parser.add_argument(
            "--log-failures",
            type=str,
            help="Directory to write per-example failure logs (created on first failure)",
        )
        parser.add_argument(
            "--max-failures",
            type=int,
            help="Stop compilation after N failures (counts each failed example). "
            "If not specified, all boards will be compiled regardless of failures.",
        )

        # Workflow options
        parser.add_argument(
            "--docker",
            action="store_true",
            help="Run compilation inside Docker container with pre-cached dependencies",
        )
        parser.add_argument(
            "--build",
            action="store_true",
            help="Build Docker image if it doesn't exist (use with --docker)",
        )
        parser.add_argument(
            "--local",
            action="store_true",
            help="Force local/native compilation, disabling Docker even if available",
        )
        parser.add_argument(
            "--run",
            action="store_true",
            help="For WASM: Run Playwright tests after compilation (default is compile-only)",
        )

        # Info options
        parser.add_argument(
            "--supported-boards",
            action="store_true",
            help="Print the list of supported boards and exit",
        )
        parser.add_argument(
            "--no-interactive",
            action="store_true",
            help="Disables interactive mode (deprecated)",
        )
        parser.add_argument(
            "--no-parallel",
            action="store_true",
            help="Disable parallel compilation (builds are already sequential, this is a no-op)",
        )
        parser.add_argument(
            "--global-cache",
            type=str,
            help="Override global PlatformIO cache directory path (for testing)",
        )

        return parser

    def _build_config(self, args: argparse.Namespace) -> CompilationConfig:
        """Build configuration from parsed arguments."""
        # Resolve boards
        boards = self._resolve_boards(args.boards)

        # Determine workflow type
        workflow = self._determine_workflow(boards, args.docker, args.local)

        # Resolve examples
        examples, skip_filters = self._resolve_examples(args, args.no_filter)

        # Parse other options
        defines: list[str] = args.defines.split(",") if args.defines else []
        extra_packages: list[str] = (
            [pkg.strip() for pkg in args.extra_packages.split(",")]
            if args.extra_packages
            else []
        )

        return CompilationConfig(
            boards=boards,
            examples=examples,
            workflow=workflow,
            defines=defines,
            extra_packages=extra_packages,
            verbose=args.verbose,
            enable_cache=(args.enable_cache or args.cache) and not args.no_cache,
            output_path=Path(args.out) if args.out else None,
            merged_bin=args.merged_bin,
            log_failures=Path(args.log_failures) if args.log_failures else None,
            max_failures=args.max_failures if hasattr(args, "max_failures") else None,
            docker_build=args.build,
            force_local=args.local,
            wasm_run=args.run,
            global_cache_dir=Path(args.global_cache) if args.global_cache else None,
            skip_filters=skip_filters,
            no_parallel=args.no_parallel,
        )

    def _resolve_boards(self, board_spec: Optional[str]) -> list[Board]:
        """Resolve board specification to Board objects.

        Returns empty list if no board specification provided - caller should
        handle default board selection based on their requirements.
        """
        if not board_spec:
            # Return empty list - caller will handle default boards
            return []

        board_names = board_spec.split(",")

        boards: list[Board] = []
        for name in board_names:
            try:
                board = create_board(name, no_project_options=False)
                boards.append(board)
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                raise ValueError(f"Failed to create board '{name}': {e}")

        return boards

    def _resolve_examples(
        self, args: argparse.Namespace, no_filter: bool = False
    ) -> tuple[list[str], bool]:
        """Resolve examples from arguments.

        Special keyword 'all' compiles all examples.
        If no examples specified, defaults to ['Blink'].

        Returns:
            Tuple of (examples, skip_filters) where skip_filters is True when filters
            should be bypassed (when specific sketches are requested or --no-filter is used)
        """
        # Track if 'all' was used for auto-discovery

        if args.positional_examples:
            examples = [self._normalize_example(ex) for ex in args.positional_examples]
        elif args.examples:
            examples = [self._normalize_example(ex) for ex in args.examples.split(",")]
        else:
            # Default to Blink if no examples specified
            examples = ["Blink"]

        # Check for special 'all' keyword
        if "all" in examples:
            # Remove 'all' keyword and discover all examples
            examples = self._discover_all_examples()

        # Apply exclusions
        if hasattr(args, "exclude_examples") and args.exclude_examples:
            excludes = set(args.exclude_examples.split(","))
            examples = [ex for ex in examples if ex not in excludes]

        # Validate examples exist
        for example in examples:
            path = self.examples_dir / example
            if not path.exists():
                raise ValueError(f"Example not found: {example}")

        # Determine filter behavior:
        # - Filters always apply UNLESS --no-filter is explicitly used
        # - This prevents compilation failures for incompatible platform/memory combinations
        # - Use --no-filter to override and attempt compilation anyway
        skip_filters = no_filter

        return examples, skip_filters

    def _normalize_example(self, example: str) -> str:
        """Normalize example name (remove 'examples/' prefix and handle case-insensitivity)."""
        if example.startswith("examples/"):
            example = example[len("examples/") :]

        # Try to find the actual directory, handling case-insensitivity
        # Always iterate to get the canonical name, even if .exists() returns True
        # (On case-insensitive filesystems like Windows, .exists() may return True for wrong-case paths)
        example_lower = example.lower()
        try:
            if self.examples_dir.exists():
                for item in self.examples_dir.iterdir():
                    if item.is_dir() and item.name.lower() == example_lower:
                        # Found the directory - return the actual canonical name
                        return item.name
        except (OSError, PermissionError):
            # If we can't iterate the directory, fall through to return original
            pass

        # If no match found, return original (will fail validation later with clear error)
        return example

    def _discover_all_examples(self) -> list[str]:
        """Discover all examples in examples directory."""
        if not self.examples_dir.exists():
            return []

        examples: list[str] = []
        for ino_file in self.examples_dir.rglob("*.ino"):
            rel_path = ino_file.parent.relative_to(self.examples_dir)
            examples.append(str(rel_path).replace("\\", "/"))

        return sorted(examples)

    def _determine_workflow(
        self, boards: list[Board], docker_flag: bool, local_flag: bool
    ) -> WorkflowType:
        """Determine workflow type from boards and flags."""
        if docker_flag and not local_flag:
            return WorkflowType.DOCKER
        if boards and boards[0].board_name == "wasm":
            return WorkflowType.WASM
        return WorkflowType.NATIVE
