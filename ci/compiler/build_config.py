"""Build configuration and build metadata for fbuild-driven board builds.

Two responsibilities:

1. ``apply_board_specific_config`` writes the per-board project ini that
   fbuild consumes (``<build_dir>/platformio.ini`` — the file name is the
   project format fbuild reads).
2. ``generate_build_info_json_from_existing_build`` writes
   ``build_info[_<example>].json`` next to it. That file is what the size,
   symbol, ELF-inspection and MCP tooling read (``prog_path``, ``aliases``,
   ``cc_flags`` ...). It used to come from a separate project-metadata
   query that cost 4.3 s per example on top of an fbuild build that already
   had every fact. Now it comes from fbuild's own outputs, in this order:

   - ``build_info_<env>.json`` that fbuild emits after a link when it knows
     the toolchain (AVR, ARM, RP2040 today), or
   - synthesis from ``.fbuild/build/<mode>/compile_commands.json`` and the
     linked ``firmware.elf`` for every other board (the ESP32 family builds
     with the LLVM toolchain and gets ``llvm-*`` aliases).
"""

import json
import shlex
import shutil
from pathlib import Path
from typing import TYPE_CHECKING, Any, NamedTuple, Optional, Sequence

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


# ---------------------------------------------------------------------------
# Tool aliases
# ---------------------------------------------------------------------------

# Binutils names as the readers expect them in ``aliases`` (ci/util/tools.py,
# ci/inspect_elf.py, ci/symbol_analysis_runner.py, mcp_server.py).
_TOOL_NAMES = [
    "gcc",
    "g++",
    "ar",
    "objcopy",
    "objdump",
    "size",
    "nm",
    "ld",
    "as",
    "ranlib",
    "strip",
    "c++filt",
    "readelf",
    "addr2line",
]

# LLVM spellings for the same tools, used when the compiler is clang.
_LLVM_TOOL_NAMES = {
    "gcc": "clang",
    "g++": "clang++",
    "ar": "llvm-ar",
    "objcopy": "llvm-objcopy",
    "objdump": "llvm-objdump",
    "size": "llvm-size",
    "nm": "llvm-nm",
    "ld": "ld.lld",
    "as": "llvm-as",
    "ranlib": "llvm-ranlib",
    "strip": "llvm-strip",
    "c++filt": "llvm-cxxfilt",
    "readelf": "llvm-readelf",
    "addr2line": "llvm-addr2line",
}


def _resolve_tool_path(value: Any, extra_path: Optional[str] = None) -> Optional[Path]:
    """Turn a compiler path from build metadata into an existing absolute path.

    Accepts absolute paths, paths relative to the cwd, and bare names that
    resolve on PATH (fbuild invokes the ESP32 LLVM toolchain as plain
    ``clang``).
    """
    if not value:
        return None
    try:
        candidate = Path(str(value))
        if candidate.is_absolute() and candidate.exists():
            return candidate
        if candidate.exists():
            return candidate.resolve()
        which_result = shutil.which(candidate.name or str(candidate), path=extra_path)
        if which_result:
            return Path(which_result)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        return None
    return None


def _target_triple(flags: list[str]) -> Optional[str]:
    """`--target=<triple>` / `-target <triple>` from a compile command, if any."""
    for i, flag in enumerate(flags):
        if flag.startswith("--target="):
            return flag[len("--target=") :]
        if flag in ("-target", "--target") and i + 1 < len(flags):
            return flags[i + 1]
    return None


def _gnu_toolchain_bin_for_target(
    triple: Any, used_paths: Sequence[str] = (), home: Optional[Path] = None
) -> Optional[Path]:
    """bin/ dir of an fbuild-cached GNU toolchain for ``triple`` (e.g.
    ``xtensa-esp-elf``), found by its ``<triple>-gcc``; None when absent.

    fbuild keeps one cache per mode (``~/.fbuild/dev``, ``~/.fbuild/prod``),
    and each can hold a different release of the same toolchain. The one the
    build used is the one whose root appears in ``used_paths`` (the compile
    command's include paths and flags), so that wins. Taking the first match
    in sort order picked ``dev`` over ``prod`` and paired a build with another
    release's binutils (FastLED#4468).
    """
    if not isinstance(triple, str) or not triple:
        return None
    base = home if home is not None else Path.home()
    try:
        found = [
            gcc
            for gcc in sorted(
                base.glob(f".fbuild/*/cache/toolchains/*/**/bin/{triple}-gcc")
            )
            if gcc.is_file()
        ]
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except OSError:
        return None
    for gcc in found:
        root = gcc.parent.parent.as_posix() + "/"
        if any(path.replace("\\", "/").startswith(root) for path in used_paths):
            return gcc.parent
    return found[0].parent if found else None


def insert_tool_aliases(
    meta_json: dict[str, dict[str, Any]], overwrite: bool = False
) -> None:
    """Fill ``aliases`` (nm, objdump, c++filt, size, ld ...) for each env.

    The toolchain bin directory and prefix are derived from ``cc_path``
    (``arm-none-eabi-gcc`` -> ``arm-none-eabi-``; ``clang`` -> ``llvm-*``).
    Existing aliases are kept unless ``overwrite`` is set, so fbuild's own
    values win when it emitted them.
    """
    for board in meta_json.keys():
        env = meta_json[board]
        aliases: dict[str, Optional[str]] = {}
        existing = env.get("aliases")
        if isinstance(existing, dict) and not overwrite:
            aliases.update({k: v for k, v in existing.items() if v})

        cc_path = _resolve_tool_path(env.get("cc_path"))
        tool_bin_dir: Optional[Path] = None
        tool_prefix = ""
        tool_suffix = ""
        use_llvm = False

        if cc_path is not None:
            cc_base = cc_path.name
            if "clang" in cc_base:
                # fbuild drives the ESP32 family through a bare `clang
                # --target=<triple>`; the matching GNU binutils (which can
                # disassemble xtensa/riscv where host llvm-objdump cannot)
                # live in fbuild's toolchain cache. Prefer those, fall back
                # to the llvm-* tools next to clang.
                used_paths = [
                    str(v)
                    for key in ("includes", "cc_flags", "cxx_flags")
                    for v in (env.get(key) or [])
                ]
                gnu_bin = _gnu_toolchain_bin_for_target(env.get("target"), used_paths)
                if gnu_bin is not None:
                    tool_bin_dir = gnu_bin
                    tool_prefix = f"{env['target']}-"
                else:
                    use_llvm = True
                    tool_bin_dir = cc_path.parent
                    tool_suffix = cc_path.suffix if cc_path.suffix == ".exe" else ""
            elif "gcc" in cc_base:
                tool_bin_dir = cc_path.parent
                tool_prefix = cc_base.split("gcc")[0]
                tool_suffix = cc_path.suffix

        for tool in _TOOL_NAMES:
            if aliases.get(tool):
                continue
            name = (
                _LLVM_TOOL_NAMES[tool] if use_llvm else f"{tool_prefix}{tool}"
            ) + tool_suffix
            resolved: Optional[str] = None
            if tool_bin_dir is not None and (tool_bin_dir / name).exists():
                resolved = str(tool_bin_dir / name)
            else:
                which_result = shutil.which(name)
                resolved = str(Path(which_result)) if which_result else None
            aliases[tool] = resolved

        env["aliases"] = aliases


# ---------------------------------------------------------------------------
# build_info.json from fbuild outputs
# ---------------------------------------------------------------------------


def _fbuild_output_dir(build_dir: Path) -> Optional[Path]:
    """Newest of ``.fbuild/build/{release,debug}`` that holds a firmware.elf."""
    root = build_dir / ".fbuild" / "build"
    candidates = [
        d for d in (root / "release", root / "debug") if (d / "firmware.elf").exists()
    ]
    if not candidates:
        return None
    return max(candidates, key=lambda d: (d / "firmware.elf").stat().st_mtime)


class CompileArgs(NamedTuple):
    """A compile command split into its flag, define and include parts."""

    flags: list[str]
    defines: list[str]
    includes: list[str]


def _split_compile_args(args: list[str]) -> CompileArgs:
    """Split a compile command into flags, defines and includes."""
    flags: list[str] = []
    defines: list[str] = []
    includes: list[str] = []
    skip_next = False
    for i, arg in enumerate(args):
        if skip_next:
            skip_next = False
            continue
        if arg.startswith("-D") and len(arg) > 2:
            defines.append(arg[2:])
        elif arg == "-D" and i + 1 < len(args):
            defines.append(args[i + 1])
            skip_next = True
        elif arg.startswith("-I") and len(arg) > 2:
            includes.append(arg[2:])
        elif arg == "-I" and i + 1 < len(args):
            includes.append(args[i + 1])
            skip_next = True
        elif arg in ("-o", "-MF", "-MQ", "-MT") and i + 1 < len(args):
            skip_next = True
        elif arg in ("-c", "-MMD", "-MD") or arg.endswith(
            (".c", ".cpp", ".cc", ".cxx", ".ino", ".S", ".s")
        ):
            continue
        else:
            flags.append(arg)
    return CompileArgs(flags, defines, includes)


def _entry_args(entry: dict[str, Any]) -> list[str]:
    if isinstance(entry.get("arguments"), list):
        return [str(a) for a in entry["arguments"]]
    command = entry.get("command")
    if isinstance(command, str) and command:
        return shlex.split(command)
    return []


def _synthesize_build_info_from_compile_commands(
    build_dir: Path, board: "Board", out_dir: Path
) -> Optional[dict[str, dict[str, Any]]]:
    """Derive a build_info env block from fbuild's compile_commands.json + ELF."""
    cc_json = out_dir / "compile_commands.json"
    if not cc_json.exists():
        return None
    try:
        entries = json.loads(cc_json.read_text())
    except (OSError, json.JSONDecodeError):
        return None
    if not isinstance(entries, list) or not entries:
        return None

    c_entry: Optional[dict[str, Any]] = None
    cxx_entry: Optional[dict[str, Any]] = None
    for entry in entries:
        file_name = str(entry.get("file", ""))
        if cxx_entry is None and file_name.endswith((".cpp", ".cc", ".cxx", ".ino")):
            cxx_entry = entry
        elif c_entry is None and file_name.endswith(".c"):
            c_entry = entry
        if c_entry is not None and cxx_entry is not None:
            break
    primary = cxx_entry or c_entry or entries[0]
    primary_args = _entry_args(primary)
    if not primary_args:
        return None

    cxx_args = _entry_args(cxx_entry) if cxx_entry else primary_args
    c_args = _entry_args(c_entry) if c_entry else primary_args
    c_parts = _split_compile_args(c_args[1:])
    cxx_parts = _split_compile_args(cxx_args[1:])
    cc_flags, defines, includes = c_parts.flags, c_parts.defines, c_parts.includes
    cxx_flags, cxx_defines, cxx_includes = (
        cxx_parts.flags,
        cxx_parts.defines,
        cxx_parts.includes,
    )
    for d in cxx_defines:
        if d not in defines:
            defines.append(d)
    for inc in cxx_includes:
        if inc not in includes:
            includes.append(inc)

    cc_path = _resolve_tool_path(c_args[0]) or Path(c_args[0])
    cxx_path = _resolve_tool_path(cxx_args[0]) or Path(cxx_args[0])

    env_block: dict[str, Any] = {
        "board": board.board_name,
        "env": board.board_name,
        "platform": getattr(board, "platform", None),
        "cc_path": str(cc_path),
        "cxx_path": str(cxx_path),
        "cc_flags": cc_flags,
        "cxx_flags": cxx_flags,
        "defines": sorted(set(defines)),
        "includes": includes,
        "libs": [],
        "link_flags": [],
        "prog_path": str((out_dir / "firmware.elf").resolve()),
        "build_dir": str(out_dir.resolve()),
        "target": _target_triple(cxx_args[1:]) or _target_triple(c_args[1:]),
        "source": "fbuild compile_commands.json",
    }
    return {board.board_name: env_block}


def generate_build_info_json_from_existing_build(
    build_dir: Path, board: "Board", example: Optional[str] = None
) -> bool:
    """Write ``build_info[_<example>].json`` for a finished fbuild build.

    Args:
        build_dir: Project directory fbuild built (holds the project ini and
            ``.fbuild/build/``)
        board: Board configuration (``board_name`` is the fbuild env)
        example: Optional example name; when given the file is
            ``build_info_<example>.json``, otherwise ``build_info.json``

    Returns:
        True if the file was written
    """
    try:
        out_dir = _fbuild_output_dir(build_dir)
        if out_dir is None:
            print(
                f"Warning: no fbuild firmware.elf under {build_dir / '.fbuild' / 'build'}; "
                "build_info.json not written"
            )
            return False

        data: Optional[dict[str, dict[str, Any]]] = None
        fbuild_info = build_dir / f"build_info_{board.board_name}.json"
        if fbuild_info.exists():
            try:
                loaded = json.loads(fbuild_info.read_text())
                if isinstance(loaded, dict) and loaded:
                    data = loaded
                    source = fbuild_info.name
            except (OSError, json.JSONDecodeError) as e:
                print(f"Warning: ignoring unreadable {fbuild_info.name}: {e}")

        if data is None:
            data = _synthesize_build_info_from_compile_commands(
                build_dir, board, out_dir
            )
            source = "compile_commands.json"
        if data is None:
            print(
                f"Warning: neither {fbuild_info.name} nor {out_dir / 'compile_commands.json'} "
                "available; build_info.json not written"
            )
            return False

        # Point every env at the ELF we just linked (fbuild's own file may
        # name firmware.hex/.bin, which the symbol tools cannot read).
        elf = out_dir / "firmware.elf"
        for env in data.values():
            env["prog_path"] = str(elf.resolve())
            fw_bin = out_dir / "firmware.bin"
            if fw_bin.exists():
                try:
                    env["prog_size"] = fw_bin.stat().st_size
                except OSError:
                    pass

        insert_tool_aliases(data)

        build_info_filename = (
            f"build_info_{example}.json" if example else "build_info.json"
        )
        build_info_path = build_dir / build_info_filename
        with open(build_info_path, "w") as f:
            json.dump(data, f, indent=4, sort_keys=True)
        print(f"Generated {build_info_filename} from {source} at {build_info_path}")
        return True

    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print(f"Warning: Exception generating build_info.json: {e}")
        return False


# ---------------------------------------------------------------------------
# Project ini
# ---------------------------------------------------------------------------


def apply_board_specific_config(
    board: "Board",
    project_ini_path: Path,
    example: str,
    paths: "FastLEDPaths",
    additional_defines: Optional[list[str]] = None,
    additional_include_dirs: Optional[list[str]] = None,
    additional_libs: Optional[list[str]] = None,
) -> bool:
    """Write the board's project ini for fbuild from the Board class.

    fbuild resolves platforms, frameworks and toolchains itself (into
    ``~/.fbuild``), so the ini carries the declared URLs untouched.
    """
    # Use provided paths object (which may have overrides)
    paths.ensure_directories_exist()

    project_root = _get_project_root()

    config_content = board.to_project_ini(
        additional_defines=additional_defines,
        additional_include_dirs=additional_include_dirs,
        additional_libs=additional_libs,
        project_root=str(project_root),
    )

    project_ini_path.write_text(config_content)

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
