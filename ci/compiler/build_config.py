"""Build configuration for FastLED PlatformIO builds."""

import json
from pathlib import Path
from typing import TYPE_CHECKING, Any, Optional

from running_process import EndOfStream, RunningProcess

from ci.compiler.build_utils import get_utf8_env
from ci.util.create_build_dir import insert_tool_aliases
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


if TYPE_CHECKING:
    from ci.boards import Board
    from ci.compiler.path_manager import FastLEDPaths


# Module-level constants
_PROJECT_ROOT: Optional[Path] = None


def _get_project_root() -> Path:
    """Get the cached project root."""
    global _PROJECT_ROOT
    if _PROJECT_ROOT is None:
        from ci.compiler.path_manager import resolve_project_root

        _PROJECT_ROOT = resolve_project_root()
    return _PROJECT_ROOT


def get_root_platformio_build_flags(board_name: str, project_root: Path) -> list[str]:
    """Read the root ``platformio.ini`` and return ``build_flags`` for the
    matching ``[env:<board_name>]`` section, with ``extends`` and ``[env]``
    inheritance resolved.

    This exists to fix issue #2664: ``bash compile --backend platformio``
    synthesises a fresh PIO project at ``.build/pio/<env>/platformio.ini``
    from ``ci/boards.py`` and was silently discarding ``build_flags`` defined
    in the user's root ``platformio.ini``. Anyone editing the root file
    reasonably expects those flags to take effect; without this merge step
    they vanish, producing subtle "flags work under fbuild but not under
    --backend=platformio" bugs.

    Behaviour:
    - Returns an empty list if the root ``platformio.ini`` is missing,
      malformed, or has no matching ``[env:<board_name>]`` section.
    - Substitution variables like ``${env:generic-esp.build_flags}`` and
      ``extends = env:generic-esp`` are resolved using the existing
      :class:`PlatformIOIni` parser, which already implements PlatformIO's
      inheritance semantics.
    - Never raises: parse errors are swallowed and logged so that a broken
      root file cannot block a build.

    Args:
        board_name: The PlatformIO env name (e.g. ``"esp32c6"``). Matches
            ``[env:<board_name>]`` in the root ini.
        project_root: FastLED project root containing ``platformio.ini``.

    Returns:
        Ordered list of build_flags strings, or ``[]`` when nothing applies.
    """
    root_ini = project_root / "platformio.ini"
    if not root_ini.exists():
        return []

    try:
        # Local import to avoid a circular import at module load.
        from ci.compiler.platformio_ini import PlatformIOIni

        pio_ini = PlatformIOIni.parseFile(root_ini)
        parsed = pio_ini.parsed
        env = parsed.environments.get(board_name)
        if env is None:
            return []
        # `parsed` already has extends + [env] inheritance applied, and
        # ${env:...} substitutions resolved (see _resolve_list_variables in
        # ci/compiler/platformio_ini.py).
        return list(env.build_flags)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        # Never let a malformed root platformio.ini block a build.
        print(
            f"Warning: failed to read root platformio.ini for env '{board_name}': {e}"
        )
        return []


def _override_prog_path_for_fbuild(
    build_dir: Path, data: dict[str, dict[str, Any]], board: "Board"
) -> None:
    """If fbuild produced a fresher ELF than PlatformIO's `prog_path`,
    rewrite the path to point at the fbuild artifact so symbol/size
    analysis reads the build we just produced — not whatever an older
    `pio run` left in `.pio/build/<env>/`.

    fbuild emits to `.fbuild/build/<env>/<release|debug>/firmware.elf`,
    where the subdir is picked by build mode (`fbuild build` defaults to
    `release/`; `fbuild build --quick` lands in `debug/`). We probe both
    candidates and pick whichever ELF is newer — that way `--quick`
    builds get the same staleness override the default mode does.

    No-op when the fbuild directory is absent (pure PlatformIO build),
    when the PIO ELF is newer than all fbuild candidates (PIO was the
    active backend), or when the metadata blob has no recognisable
    environment entry.
    """
    fbuild_root = build_dir / ".fbuild" / "build"
    if not fbuild_root.is_dir():
        return

    if not data:
        return

    env_name = board.board_name if board.board_name in data else next(iter(data), None)
    if env_name is None:
        return
    env = data[env_name]

    # fbuild lays out artifacts under <env>/<release|debug>/firmware.elf.
    # `fbuild build`           -> release/
    # `fbuild build --release` -> release/
    # `fbuild build --quick`   -> debug/
    # Pick whichever variant is newer so users running --quick still get
    # a fresh staleness override (Closes #2852).
    fbuild_elf_candidates = [
        fbuild_root / env_name / "release" / "firmware.elf",
        fbuild_root / env_name / "debug" / "firmware.elf",
    ]
    existing = [c for c in fbuild_elf_candidates if c.exists()]
    if not existing:
        return
    fbuild_elf = max(existing, key=lambda p: p.stat().st_mtime)

    current_prog_raw = env.get("prog_path")
    if isinstance(current_prog_raw, str) and current_prog_raw:
        current_prog = Path(current_prog_raw)
        # Only override when fbuild's ELF is strictly newer (or PIO's is
        # missing). A successful `pio run` that beats the most recent
        # fbuild run should win.
        if current_prog.exists():
            try:
                if current_prog.stat().st_mtime >= fbuild_elf.stat().st_mtime:
                    return
            except OSError:
                pass

    fbuild_elf_str = str(fbuild_elf.resolve())
    env["prog_path"] = fbuild_elf_str
    fbuild_bin = fbuild_elf.with_suffix(".bin")
    if fbuild_bin.exists() and "prog_size" in env:
        try:
            env["prog_size"] = fbuild_bin.stat().st_size
        except OSError:
            pass
    print(
        f"  Pointed prog_path at fbuild artifact: {fbuild_elf_str} "
        f"(was {current_prog_raw!r})"
    )


def generate_build_info_json_from_existing_build(
    build_dir: Path, board: "Board", example: Optional[str] = None
) -> bool:
    """Generate build_info.json from an existing PlatformIO build.

    Args:
        build_dir: Build directory containing the PlatformIO project
        board: Board configuration
        example: Optional example name for generating example-specific build_info_{example}.json

    Returns:
        True if build_info.json was successfully generated
    """
    try:
        # Use existing project to get metadata with streaming output
        # This prevents the process from appearing to stall during library resolution
        metadata_cmd = ["pio", "project", "metadata", "--json-output"]

        print("Generating build metadata (resolving library dependencies)...")

        metadata_proc = RunningProcess(
            metadata_cmd,
            cwd=build_dir,
            auto_run=True,
            timeout=None,  # No global timeout - rely on per-line timeout instead
            env=get_utf8_env(),
        )

        # Stream output while collecting it for JSON parsing
        # Show progress indicators for long-running operations
        # Use per-line timeout of 15 minutes - some packages take a long time to download
        metadata_lines: list[str] = []
        while line := metadata_proc.get_next_line(timeout=900):
            if isinstance(line, EndOfStream):
                break
            line_str = str(line)
            metadata_lines.append(line_str)
            # Show progress for library resolution operations (not JSON output)
            # JSON output will be a single line starting with '{'
            if not line_str.strip().startswith("{"):
                # This is a progress message from PlatformIO
                stripped = line_str.strip()
                if any(
                    keyword in stripped
                    for keyword in [
                        "Resolving",
                        "Installing",
                        "Downloading",
                        "Checking",
                    ]
                ):
                    print(f"  {stripped}")

        metadata_proc.wait()

        if metadata_proc.returncode != 0:
            # Note: RunningProcess merges stderr into stdout
            print(
                f"Warning: Failed to get metadata for build_info.json (exit code {metadata_proc.returncode})"
            )
            return False

        # Parse and save the metadata
        try:
            metadata_output = "".join(metadata_lines)
            data = json.loads(metadata_output)

            # When the build was driven by fbuild (which writes its ELF to
            # `<build_dir>/.fbuild/build/<env>/release/firmware.elf`), the
            # `prog_path` we just got from `pio project metadata` still
            # points at the PlatformIO output path under `.pio/build/<env>/`.
            # That `.pio/` ELF is whatever an older `pio run` left there
            # (often empty or stale), so downstream symbol/size analysis
            # would silently analyze the wrong binary. Detect the fresher
            # fbuild ELF and rewrite `prog_path` to it so the consumers
            # (ci/symbol_analysis_runner.py, ci/inspect_elf.py,
            # ci/compiled_size.py firmware.bin fallback) see the build we
            # just made.
            _override_prog_path_for_fbuild(build_dir, data, board)

            # Add tool aliases for symbol analysis and debugging
            insert_tool_aliases(data)

            # Save to build_info.json (example-specific if example provided)
            if example:
                build_info_filename = f"build_info_{example}.json"
            else:
                build_info_filename = "build_info.json"
            build_info_path = build_dir / build_info_filename
            with open(build_info_path, "w") as f:
                json.dump(data, f, indent=4, sort_keys=True)

            print(f"✅ Generated {build_info_filename} at {build_info_path}")
            return True

        except json.JSONDecodeError as e:
            print(f"Warning: Failed to parse metadata JSON for build_info.json: {e}")
            return False

    except TimeoutError:
        print("Warning: Timeout generating build_info.json (no output for 900s)")
        return False
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print(f"Warning: Exception generating build_info.json: {e}")
        return False


def apply_board_specific_config(
    board: "Board",
    platformio_ini_path: Path,
    example: str,
    paths: "FastLEDPaths",
    additional_defines: Optional[list[str]] = None,
    additional_include_dirs: Optional[list[str]] = None,
    additional_libs: Optional[list[str]] = None,
) -> bool:
    """Apply board-specific build configuration from Board class."""
    # Use provided paths object (which may have overrides)
    paths.ensure_directories_exist()

    project_root = _get_project_root()

    # Generate platformio.ini content using the enhanced Board method
    config_content = board.to_platformio_ini(
        additional_defines=additional_defines,
        additional_include_dirs=additional_include_dirs,
        additional_libs=additional_libs,
        include_platformio_section=True,
        core_dir=str(paths.core_dir),
        packages_dir=str(paths.packages_dir),
        project_root=str(project_root),
        build_cache_dir=str(paths.build_cache_dir),
    )

    # Apply PlatformIO cache optimization to speed up builds
    try:
        from ci.compiler.platformio_cache import PlatformIOCache
        from ci.compiler.platformio_ini import PlatformIOIni

        # Parse the generated INI content
        pio_ini = PlatformIOIni.parseString(config_content)

        # Set up global PlatformIO cache
        cache = PlatformIOCache(paths.global_platformio_cache_dir)

        # Optimize by downloading and caching packages, replacing URLs with local file:// paths
        pio_ini.optimize(cache)

        # Use the optimized content
        config_content = str(pio_ini)
        print(
            f"Applied PlatformIO cache optimization using cache directory: {paths.global_platformio_cache_dir}"
        )

    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        # Graceful fallback to original URLs on cache failures
        print(
            f"Warning: PlatformIO cache optimization failed, using original URLs: {e}"
        )
        # config_content remains unchanged (original URLs)

    platformio_ini_path.write_text(config_content)

    # Log applied configurations for debugging
    if board.build_flags:
        print(f"Applied build_flags: {board.build_flags}")
    if board.defines:
        print(f"Applied defines: {board.defines}")
    if additional_defines:
        print(f"Applied additional defines: {additional_defines}")
    if additional_include_dirs:
        print(f"Applied additional include dirs: {additional_include_dirs}")
    if board.platform_packages:
        print(f"Using platform_packages: {board.platform_packages}")

    return True
