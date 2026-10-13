"""Pinned real ESP SDK environments for semantic lint (FastLED #4829).

Provision explicitly with ``uv run python -m ci.tools.ast_profiles --provision``.
Normal lint only reads the resulting environment; it never downloads packages
or builds firmware. fbuild remains the authority for SDK compile settings.
"""

import argparse
import hashlib
import json
import os
import re
import shlex
import shutil
from dataclasses import asdict, dataclass
from importlib.metadata import version
from pathlib import Path
from typing import cast

from running_process import RunningProcess

from ci.util.paths import PROJECT_ROOT


SDFAT_VERSION = "2.3.1"
PROFILE_ROOT = PROJECT_ROOT / ".build" / "ast-profiles"
MANIFEST_NAME = "ast-environment.json"
SCHEMA_VERSION = 3
QUERY_MANIFEST_NAME = "target-clang-query.json"
HOST_MANIFEST_NAME = "host-environment.json"
INCLUDE_ENVIRONMENT = (
    "CPATH",
    "CPLUS_INCLUDE_PATH",
    "C_INCLUDE_PATH",
    "SDKROOT",
    "INCLUDE",
    "OBJC_INCLUDE_PATH",
    "OBJCPLUS_INCLUDE_PATH",
    "NIX_CFLAGS_COMPILE",
    "GCC_EXEC_PREFIX",
    "COMPILER_PATH",
)


class ProfileError(RuntimeError):
    """A required SDK environment is missing, incomplete, or inconsistent."""


@dataclass(frozen=True)
class TargetQuery:
    executable: Path
    version: str
    identity_file: Path


@dataclass(frozen=True)
class HostEnvironment:
    query: tuple[str, ...]
    version: str
    arguments: tuple[str, ...]
    include_paths: tuple[Path, ...]
    identity_files: tuple[Path, ...]


def target_process_environment() -> dict[str, str]:
    """Keep target analysis independent of host SDK/header injection."""
    return {
        key: value
        for key, value in os.environ.items()
        if key not in INCLUDE_ENVIRONMENT
    }


def _include_environment(*, target: bool = False) -> dict[str, str | None]:
    environment = target_process_environment() if target else os.environ
    return {key: environment.get(key) for key in INCLUDE_ENVIRONMENT}


def _content_digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _derivation_inputs(paths: tuple[Path, ...]) -> dict[str, str]:
    return {
        str(path.resolve()): _content_digest(path) for path in (*paths, Path(__file__))
    }


def _validate_derivation(payload: dict[str, object]) -> None:
    inputs = payload.get("derivation_inputs")
    if not isinstance(inputs, dict) or not inputs:
        raise ProfileError("Missing AST derivation identities; provision again")
    for path, expected in inputs.items():
        if not isinstance(path, str) or not isinstance(expected, str):
            raise ProfileError("Invalid AST derivation identities; provision again")
        try:
            actual = _content_digest(Path(path))
        except OSError as error:
            raise ProfileError(
                f"Missing AST derivation input {path}; provision again"
            ) from error
        if actual != expected:
            raise ProfileError(f"AST derivation input changed: {path}; provision again")


def provision_host_profile(root: Path = PROFILE_ROOT) -> HostEnvironment:
    """Discover the actual host wrapper's implicit includes once, explicitly."""
    from ci.tools.check_noexcept import (
        _compiler_args,
        _find_clang_query,
        _raise_on_query_errors,
    )

    query = _find_clang_query()
    if not query:
        raise ProfileError(
            "Host clang-query unavailable; install the required Clang tools"
        )
    root.mkdir(parents=True, exist_ok=True)
    source = root / "host-includes.cpp"
    source.write_text("// Host include discovery; no SDK stubs.\n", encoding="utf-8")
    arguments = tuple(_compiler_args())
    output = _run([*query, str(source), "--", *arguments, "-v"], PROJECT_ROOT)
    _raise_on_query_errors(0, output)
    paths = parse_implicit_includes(output)
    query_version = _run([*query, "--version"], PROJECT_ROOT)
    executable = Path(shutil.which(query[0]) or query[0]).resolve()
    manifest = root / HOST_MANIFEST_NAME
    query_files = [executable]
    if any("clang-tool-chain-clang-query" in part for part in query):
        bundled = load_target_query(root)
        query_files.extend([bundled.executable, bundled.identity_file])
    unwrapped = executable.with_name("clang-query-unwrapped")
    if unwrapped.is_file():
        query_files.append(unwrapped)
    payload = {
        "schema": SCHEMA_VERSION,
        "query": query,
        "version": query_version,
        "arguments": list(arguments),
        "include_paths": [str(path) for path in paths],
        "executable": str(executable),
        "query_files": [str(path) for path in query_files],
        "environment": _include_environment(),
        "derivation_inputs": _derivation_inputs(tuple(query_files)),
    }
    manifest.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return HostEnvironment(
        tuple(query), query_version, arguments, paths, (manifest, *query_files)
    )


def load_host_profile(root: Path = PROFILE_ROOT) -> HostEnvironment:
    """Load host preprocessing inputs without starting a process."""
    from ci.tools.check_noexcept import _compiler_args, _find_clang_query

    manifest = root / HOST_MANIFEST_NAME
    if not manifest.is_file():
        raise ProfileError("Missing host AST environment; run ast_profiles --provision")
    payload = cast(dict[str, object], json.loads(manifest.read_text(encoding="utf-8")))
    _validate_derivation(payload)
    query = tuple(cast(list[str], payload.get("query", [])))
    arguments = tuple(cast(list[str], payload.get("arguments", [])))
    executable = Path(str(payload.get("executable", "")))
    if (
        payload.get("schema") != SCHEMA_VERSION
        or payload.get("environment") != _include_environment()
        or query != tuple(_find_clang_query() or [])
        or arguments != tuple(_compiler_args())
        or not executable.is_file()
        or Path(shutil.which(query[0]) or query[0]).resolve() != executable
    ):
        raise ProfileError("Host AST environment changed; run ast_profiles --provision")
    paths = tuple(Path(path) for path in cast(list[str], payload["include_paths"]))
    query_files = tuple(
        Path(path) for path in cast(list[str], payload.get("query_files", []))
    )
    if not query_files or any(not path.is_file() for path in query_files):
        raise ProfileError("Host query implementation missing; provision again")
    if any(not path.is_dir() for path in paths):
        raise ProfileError("Host compiler include directory missing; provision again")
    return HostEnvironment(
        query, str(payload["version"]), arguments, paths, (manifest, *query_files)
    )


def provision_target_query(root: Path = PROFILE_ROOT) -> TargetQuery:
    """Install the target-capable bundled query once, outside normal lint."""
    from clang_tool_chain.execution.clang_extra import find_clang_extra_tool

    executable = find_clang_extra_tool("clang-query").resolve()
    query_version = _run([str(executable), "--version"], PROJECT_ROOT)
    root.mkdir(parents=True, exist_ok=True)
    manifest = root / QUERY_MANIFEST_NAME
    manifest.write_text(
        json.dumps(
            {
                "schema": SCHEMA_VERSION,
                "provider": "clang-tool-chain",
                "package_version": version("clang-tool-chain"),
                "executable": str(executable),
                "version": query_version,
                "derivation_inputs": _derivation_inputs((executable,)),
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return TargetQuery(executable, query_version, manifest)


def load_target_query(root: Path = PROFILE_ROOT) -> TargetQuery:
    """Load only the provisioned bundled binary; never search PATH or download."""
    manifest = root / QUERY_MANIFEST_NAME
    if not manifest.is_file():
        raise ProfileError(
            "Missing target clang-query; run uv run python -m ci.tools.ast_profiles --provision"
        )
    from clang_tool_chain.path_utils import get_clang_extra_install_dir
    from clang_tool_chain.platform.detection import get_platform_info

    platform, architecture = get_platform_info()
    name = "clang-query.exe" if platform == "win" else "clang-query"
    expected = (
        get_clang_extra_install_dir(platform, architecture) / "bin" / name
    ).resolve()
    payload = cast(dict[str, object], json.loads(manifest.read_text(encoding="utf-8")))
    _validate_derivation(payload)
    executable = Path(str(payload.get("executable", "")))
    if (
        payload.get("schema") != SCHEMA_VERSION
        or payload.get("provider") != "clang-tool-chain"
        or payload.get("package_version") != version("clang-tool-chain")
        or executable != expected
        or not executable.is_file()
        or not payload.get("version")
    ):
        raise ProfileError(
            "Target clang-query identity changed or is missing; provision again"
        )
    return TargetQuery(executable, str(payload["version"]), manifest)


@dataclass(frozen=True)
class EspProfile:
    name: str
    board: str
    platform: str
    target: str
    idf_major: int


ESP_PROFILES = (
    EspProfile("esp32-idf4", "esp32dev", "espressif32@4.4.0", "xtensa-esp32-elf", 4),
    EspProfile(
        "esp32-idf5",
        "esp32dev",
        "https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip",
        "xtensa-esp32-elf",
        5,
    ),
    EspProfile(
        "esp32c6-idf5",
        "esp32-c6-devkitc-1",
        "https://github.com/pioarduino/platform-espressif32/releases/download/55.03.35/platform-espressif32.zip",
        "riscv32-esp-elf",
        5,
    ),
)


@dataclass(frozen=True)
class CompileCommand:
    arguments: tuple[str, ...]
    directory: Path
    source: Path
    database: Path


@dataclass(frozen=True)
class AdaptedArguments:
    arguments: tuple[str, ...]
    removed: tuple[str, ...]
    response_files: tuple[Path, ...]


@dataclass(frozen=True)
class ProfileEnvironment:
    profile: EspProfile
    project_dir: Path
    compiler: Path
    compiler_version: str
    arguments: tuple[str, ...]
    include_paths: tuple[Path, ...]
    identity_files: tuple[Path, ...]
    removed_arguments: tuple[str, ...]
    dependency_roots: tuple[Path, ...] = ()

    def backend_arguments(self, backend: str = "sd") -> tuple[str, ...]:
        if backend not in ("sd", "sdfat"):
            raise ProfileError(f"Unknown SD backend: {backend}")
        return self.arguments + (("-DUSE_SDFAT=1",) if backend == "sdfat" else ())


def project_ini(profile: EspProfile) -> str:
    """SDK-only project; actual FastLED sources are always read from checkout."""
    return (
        f"[env:{profile.name}]\nboard = {profile.board}\n"
        f"platform = {profile.platform}\nframework = arduino\n"
        "lib_deps =\n"
        f"    greiman/SdFat@{SDFAT_VERSION}\n"
        "lib_ldf_mode = chain\n"
    )


def _absolute(value: str, directory: Path) -> Path:
    path = Path(value)
    return path.resolve() if path.is_absolute() else (directory / path).resolve()


def _expand_response_files(
    arguments: list[str], directory: Path, seen: frozenset[Path] = frozenset()
) -> tuple[list[str], list[Path]]:
    expanded: list[str] = []
    files: list[Path] = []
    for arg in arguments:
        if not arg.startswith("@"):
            expanded.append(arg)
            continue
        path = _absolute(arg[1:], directory)
        if path in seen or not path.is_file():
            raise ProfileError(f"Missing or cyclic compiler response file: {path}")
        nested, nested_files = _expand_response_files(
            shlex.split(path.read_text(encoding="utf-8")), directory, seen | {path}
        )
        expanded.extend(nested)
        files.extend([path, *nested_files])
    return expanded, files


# Only known GCC code-generation switches are removed. Unknown switches must
# reach Clang and fail visibly, rather than quietly changing target semantics.
_GCC_ONLY = frozenset(
    {
        "-mlongcalls",
        "-fno-tree-switch-conversion",
        "-fstrict-volatile-bitfields",
        "-fno-shrink-wrap",
        "-mdisable-hardware-atomics",
        "-freorder-blocks",
    }
)
_PATH_FLAGS = ("-isystem", "-iquote", "-idirafter", "-include", "-imacros", "-I")
_OUTPUT_FLAGS = ("-o", "-MF", "-MT", "-MQ")


def adapt_arguments(
    command: list[str] | tuple[str, ...], directory: Path, source: Path
) -> AdaptedArguments:
    """Adapt raw fbuild argv, preserving defines, ABI, and real SDK paths."""
    if not command:
        raise ProfileError("Empty compiler command")
    argv, response_files = _expand_response_files(list(command[1:]), directory)
    result: list[str] = []
    removed: list[str] = []
    index = 0
    while index < len(argv):
        arg = argv[index]
        index += 1
        if arg in _OUTPUT_FLAGS:
            if index == len(argv):
                raise ProfileError(f"Missing value after {arg}")
            removed.extend([arg, argv[index]])
            index += 1
            continue
        if (
            arg in ("-c", "-MMD", "-MD", "-MP")
            or arg in _GCC_ONLY
            or arg.startswith(("-mfix-", "-Wl,", "-flto", "-fuse-linker-plugin"))
        ):
            removed.append(arg)
            continue
        if any(arg.startswith(flag) and arg != flag for flag in _OUTPUT_FLAGS):
            removed.append(arg)
            continue
        if not arg.startswith("-") and _absolute(arg, directory) == source.resolve():
            removed.append(arg)
            continue
        if arg in _PATH_FLAGS:
            if index == len(argv):
                raise ProfileError(f"Missing path after {arg}")
            result.extend([arg, str(_absolute(argv[index], directory))])
            index += 1
            continue
        path_flag = next(
            (flag for flag in _PATH_FLAGS if arg.startswith(flag) and arg != flag), None
        )
        if path_flag:
            result.append(path_flag + str(_absolute(arg[len(path_flag) :], directory)))
        elif arg.startswith("-D"):
            # fbuild's raw DB quotes some definitions for shell execution even
            # in argv form. Undo this one escaping layer, not arbitrary escapes.
            result.append(arg.replace('\\"', '"'))
        else:
            result.append(arg)
    return AdaptedArguments(tuple(result), tuple(removed), tuple(response_files))


def read_compile_command(project_dir: Path) -> CompileCommand:
    database = project_dir / ".fbuild/build/release/compile_commands.raw.json"
    if not database.is_file():
        raise ProfileError(f"Missing {database}; run ast_profiles --provision")
    rows = cast(
        list[dict[str, object]], json.loads(database.read_text(encoding="utf-8"))
    )
    expected = (project_dir / "src/main.cpp").resolve()
    for row in rows:
        directory = Path(str(row["directory"])).resolve()
        source = _absolute(str(row["file"]), directory)
        if source != expected:
            continue
        argv = row.get("arguments")
        if not isinstance(argv, list) or not all(isinstance(arg, str) for arg in argv):
            raise ProfileError(f"Raw database lacks structured arguments for {source}")
        return CompileCommand(tuple(cast(list[str], argv)), directory, source, database)
    raise ProfileError(f"No SDK probe translation unit in {database}; provision again")


def parse_implicit_includes(output: str) -> tuple[Path, ...]:
    marker = "#include <...> search starts here:"
    if marker not in output or "End of search list." not in output:
        raise ProfileError("Compiler did not report its implicit include search paths")
    section = output.split(marker, 1)[1].split("End of search list.", 1)[0]
    paths = tuple(
        Path(line.strip().removesuffix(" (framework directory)")).resolve()
        for line in section.splitlines()
        if line.strip()
    )
    if not paths or any(not path.is_dir() for path in paths):
        raise ProfileError("Compiler implicit include directories are missing")
    return paths


def target_type_definitions(output: str) -> tuple[str, ...]:
    """Preserve the target GCC's integer typedef choices in SDK headers.

    RV32 GCC uses long int for int32_t whereas Clang defaults to int. Both
    have 32-bit storage, but C++ overload resolution distinguishes the types.
    Reuse only emitted type aliases; never pretend Clang is a different
    compiler by replacing feature/version/architecture predefinitions.
    """
    aliases = dict(
        re.findall(r"^#define (__[A-Z0-9_]+_TYPE__) (.+)$", output, re.MULTILINE)
    )
    if not {
        "__INT32_TYPE__",
        "__UINT32_TYPE__",
        "__SIZE_TYPE__",
        "__INTPTR_TYPE__",
    }.issubset(aliases):
        raise ProfileError("Target compiler did not emit required integer type aliases")
    return tuple(
        arg
        for name, spelling in sorted(aliases.items())
        for arg in (f"-U{name}", f"-D{name}={spelling}")
    )


def target_abi_arguments(output: str) -> tuple[str, ...]:
    """Select builtin type semantics from the target GCC's emitted evidence."""
    macros = dict(re.findall(r"^#define (\w+) (.+)$", output, re.MULTILINE))
    wchar = (macros.get("__SIZEOF_WCHAR_T__"), "__WCHAR_UNSIGNED__" in macros)
    if wchar == ("2", True):
        wchar_arguments = ("-fshort-wchar",)
    elif wchar == ("4", False):
        wchar_arguments = ("-fno-short-wchar",)
    else:
        raise ProfileError(f"Unsupported target wchar_t ABI: {wchar}")
    return (
        *wchar_arguments,
        "-funsigned-char" if "__CHAR_UNSIGNED__" in macros else "-fsigned-char",
    )


def validate_target_abi(
    directory: Path, output: str, arguments: tuple[str, ...]
) -> tuple[Path, ...]:
    """Reject adapters whose builtin C++ types disagree with the real compiler.

    Type alias predefinitions alone cannot change builtin wchar_t or sizeof.
    Compile assertions before publishing a profile, rather than trusting those
    predefinitions to stand in for a compatible target ABI.
    """
    from ci.tools.check_ast_combined import analyze_translation_unit
    from ci.tools.check_noexcept import NoexceptCheckError

    macros = dict(re.findall(r"^#define (\w+) (.+)$", output, re.MULTILINE))
    lines = [
        "#include <stdint.h>",
        "#include <stddef.h>",
        "template<class A, class B> struct same { static constexpr bool value = false; };",
        "template<class A> struct same<A, A> { static constexpr bool value = true; };",
    ]
    sizes = (
        ("int", "__SIZEOF_INT__"),
        ("long", "__SIZEOF_LONG__"),
        ("void*", "__SIZEOF_POINTER__"),
        ("size_t", "__SIZEOF_SIZE_T__"),
        ("intptr_t", "__SIZEOF_POINTER__"),
        ("wchar_t", "__SIZEOF_WCHAR_T__"),
        ("long double", "__SIZEOF_LONG_DOUBLE__"),
    )
    aliases = (
        ("size_t", "__SIZE_TYPE__"),
        ("intptr_t", "__INTPTR_TYPE__"),
        ("int32_t", "__INT32_TYPE__"),
    )
    required = {macro for _, macro in (*sizes, *aliases)} | {"__CHAR_BIT__"}
    if missing := required - macros.keys():
        raise ProfileError(f"Target ABI evidence missing: {sorted(missing)}")
    for spelling, macro in sizes:
        lines.append(
            f'static_assert(sizeof({spelling}) == {macros[macro]}, "{spelling} size");'
        )
    lines.append(
        f'static_assert(sizeof(int32_t) * {macros["__CHAR_BIT__"]} == 32, "int32_t size");'
    )
    for spelling, unsigned in (
        ("int", False),
        ("long", False),
        ("size_t", True),
        ("intptr_t", False),
        ("int32_t", False),
        ("wchar_t", "__WCHAR_UNSIGNED__" in macros),
        ("char", "__CHAR_UNSIGNED__" in macros),
    ):
        lines.append(
            f"static_assert((static_cast<{spelling}>(-1) > 0) == "
            f'{str(unsigned).lower()}, "{spelling} signedness");'
        )
    for spelling, macro in aliases:
        lines.append(
            f'static_assert(same<{spelling}, {macros[macro]}>::value, "{spelling} alias");'
        )
    lines.append(
        'static_assert(same<size_t, decltype(sizeof(0))>::value, "builtin size_t");'
    )
    probe = directory / "target-abi.cpp"
    probe.write_text("\n".join(lines) + "\n", encoding="utf-8")
    query = load_target_query()
    try:
        analyze_translation_unit(
            [str(query.executable)],
            str(probe),
            ".*",
            list(arguments),
            process_env=target_process_environment(),
        )
    except NoexceptCheckError as error:
        raise ProfileError(f"Target C++ ABI validation failed: {error}") from error
    return (probe, query.executable, query.identity_file)


def _run(
    command: list[str],
    directory: Path,
    timeout: int = 180,
    *,
    process_env: dict[str, str] | None = None,
) -> str:
    result = RunningProcess.run(
        command,
        cwd=str(directory),
        input="",
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
        env=process_env,
    )
    if result.returncode:
        raise ProfileError(
            f"Command failed ({result.returncode}): {command}\n{result.stdout or ''}"
        )
    return result.stdout or ""


def _include_paths(arguments: tuple[str, ...]) -> tuple[Path, ...]:
    paths: list[Path] = []
    index = 0
    flags = ("-isystem", "-iquote", "-idirafter", "-I")
    while index < len(arguments):
        arg = arguments[index]
        index += 1
        if arg in flags:
            paths.append(Path(arguments[index]))
            index += 1
            continue
        flag = next((flag for flag in flags if arg.startswith(flag)), None)
        if flag:
            paths.append(Path(arg[len(flag) :]))
    return tuple(dict.fromkeys(paths))


def _required_header(name: str, paths: tuple[Path, ...]) -> Path:
    for directory in paths:
        path = directory / name
        if path.is_file():
            return path
    raise ProfileError(
        f"Required SDK header {name} unavailable; provision the ESP profiles"
    )


def dependency_roots(
    packages: str, profile_name: str, include_paths: tuple[Path, ...]
) -> tuple[Path, ...]:
    """Snapshot real SDK header trees without adding compiler search paths.

    SDK headers use relative includes into sibling components outside their
    own -I directory. fbuild's package receipts establish the installed roots;
    expand to their first include/ ancestor, excluding multi-GB binary trees.
    """
    receipt = json.loads(packages)
    origins = [
        Path(package["install_path"]).resolve()
        for environment in receipt["environments"]
        if environment["env"] == profile_name
        for package in environment["packages"]
        if package.get("install_path")
        and package.get("kind") in {"framework", "toolchain", "library"}
    ]
    roots: set[Path] = set()
    for path in include_paths:
        resolved = path.resolve()
        root = resolved
        for origin in origins:
            if not resolved.is_relative_to(origin):
                continue
            relative = resolved.relative_to(origin)
            if "include" in relative.parts:
                root = origin.joinpath(
                    *relative.parts[: relative.parts.index("include") + 1]
                )
            elif "libraries" in relative.parts:
                index = relative.parts.index("libraries")
                root = origin.joinpath(*relative.parts[: index + 2])
            break
        roots.add(root)
    # Do not repeatedly walk children when the authoritative ancestor is present.
    return tuple(
        sorted(
            root
            for root in roots
            if not any(root != other and root.is_relative_to(other) for other in roots)
        )
    )


def _validate_environment(environment: ProfileEnvironment) -> None:
    if not environment.compiler.is_file():
        raise ProfileError(f"Missing compiler {environment.compiler}; provision again")
    for path in environment.identity_files:
        if not path.is_file():
            raise ProfileError(
                f"Missing SDK/configuration file {path}; provision again"
            )
    for path in environment.include_paths:
        normalized = path.as_posix()
        if normalized in (
            "/usr/include",
            "/usr/local/include",
        ) or normalized.startswith("/usr/include/c++/"):
            raise ProfileError(f"Host headers leaked into ESP analysis: {path}")
    idf = _required_header("esp_idf_version.h", environment.include_paths)
    match = re.search(
        r"#\s*define\s+ESP_IDF_VERSION_MAJOR\s+(\d+)", idf.read_text(encoding="utf-8")
    )
    if match is None or int(match[1]) != environment.profile.idf_major:
        raise ProfileError(
            f"Unexpected IDF major for {environment.profile.name}: {idf}"
        )
    for header in ("Arduino.h", "SD.h", "FS.h", "SPI.h", "SdFat.h", "sdkconfig.h"):
        _required_header(header, environment.include_paths)


def provision_profile(
    profile: EspProfile, project_dir: Path | None = None
) -> ProfileEnvironment:
    """Resolve real SDK dependencies and generate argv, without compiling firmware."""
    directory = (project_dir or PROFILE_ROOT / profile.name).resolve()
    process_env = target_process_environment()
    (directory / "src").mkdir(parents=True, exist_ok=True)
    ini = directory / "platformio.ini"
    ini.write_text(project_ini(profile), encoding="utf-8")
    (directory / "src/main.cpp").write_text(
        "#include <Arduino.h>\n#include <SD.h>\n#include <SdFat.h>\nvoid setup() {}\nvoid loop() {}\n",
        encoding="utf-8",
    )
    from ci.util.fbuild_runner import get_fbuild_executable

    fbuild = get_fbuild_executable()
    if not fbuild:
        raise ProfileError("fbuild unavailable; run uv sync before provisioning")
    packages = _run(
        [fbuild, "install", str(directory), "-e", profile.name, "--json"],
        PROJECT_ROOT,
        1800,
        process_env=process_env,
    )
    package_file = directory / "packages.json"
    package_file.write_text(packages, encoding="utf-8")
    _run(
        [
            fbuild,
            "build",
            str(directory),
            "-e",
            profile.name,
            "--release",
            "-t",
            "compiledb",
        ],
        PROJECT_ROOT,
        1800,
        process_env=process_env,
    )
    command = read_compile_command(directory)
    executable = shutil.which(command.arguments[0])
    compiler = (
        Path(executable)
        if executable
        else _absolute(command.arguments[0], command.directory)
    )
    if not compiler.is_file():
        raise ProfileError(f"SDK compiler unavailable: {compiler}")
    adapted = adapt_arguments(command.arguments, command.directory, command.source)
    # Preserve ABI selection for GCC's multilib include discovery. Empty stdin
    # works on Windows too; /dev/null does not.
    abi = [
        arg
        for arg in adapted.arguments
        if arg.startswith(("-march=", "-mabi=", "-mcpu="))
        or arg
        in ("-fshort-wchar", "-fno-short-wchar", "-fsigned-char", "-funsigned-char")
    ]
    implicit = parse_implicit_includes(
        _run(
            [str(compiler), *abi, "-E", "-x", "c++", "-v", "-"],
            command.directory,
            process_env=process_env,
        )
    )
    macros = _run(
        [str(compiler), *abi, "-dM", "-E", "-x", "c++", "-"],
        command.directory,
        process_env=process_env,
    )
    macro_file = directory / "compiler-predefines.txt"
    macro_file.write_text(macros, encoding="utf-8")
    type_arguments = target_type_definitions(macros)
    arguments = (
        f"-I{PROJECT_ROOT / 'src'}",
        *adapted.arguments,
        f"--target={profile.target}",
        "-nostdinc",
        "-nostdinc++",
        *target_abi_arguments(macros),
        *type_arguments,
        *(arg for path in implicit for arg in ("-isystem", str(path))),
    )
    paths = _include_paths(arguments)
    identity_files = (
        ini,
        command.database,
        package_file,
        macro_file,
        *adapted.response_files,
        *(
            _required_header(name, paths)
            for name in (
                "esp_idf_version.h",
                "esp_arduino_version.h",
                "sdkconfig.h",
                "SdFat.h",
            )
        ),
    )
    environment = ProfileEnvironment(
        profile,
        directory,
        compiler,
        _run([str(compiler), "--version"], command.directory, process_env=process_env),
        arguments,
        paths,
        identity_files,
        adapted.removed,
        dependency_roots(packages, profile.name, paths),
    )
    _validate_environment(environment)
    abi_identity_files = validate_target_abi(directory, macros, arguments)
    payload = {
        "schema": SCHEMA_VERSION,
        "profile": asdict(profile),
        "compiler": str(compiler),
        "compiler_version": environment.compiler_version,
        "arguments": list(arguments),
        "include_paths": [str(path) for path in paths],
        "identity_files": [str(path) for path in identity_files],
        "removed_arguments": list(adapted.removed),
        "environment": _include_environment(target=True),
        "dependency_roots": [str(path) for path in environment.dependency_roots],
        "derivation_inputs": _derivation_inputs(
            (
                compiler,
                ini,
                command.database,
                package_file,
                macro_file,
                *adapted.response_files,
                *abi_identity_files,
            )
        ),
    }
    (directory / MANIFEST_NAME).write_text(
        json.dumps(payload, indent=2) + "\n", encoding="utf-8"
    )
    return environment


def load_profile(
    profile: EspProfile, project_dir: Path | None = None
) -> ProfileEnvironment:
    """Read required prerequisites; never provision or invoke a compiler here."""
    directory = (project_dir or PROFILE_ROOT / profile.name).resolve()
    manifest = directory / MANIFEST_NAME
    if not manifest.is_file():
        raise ProfileError(
            f"Missing ESP profile {profile.name}. Run: uv run python -m ci.tools.ast_profiles --provision"
        )
    ini = directory / "platformio.ini"
    if not ini.is_file() or ini.read_text(encoding="utf-8") != project_ini(profile):
        raise ProfileError(
            f"Changed ESP configuration for {profile.name}; provision again"
        )
    payload = cast(dict[str, object], json.loads(manifest.read_text(encoding="utf-8")))
    _validate_derivation(payload)
    if (
        payload.get("schema") != SCHEMA_VERSION
        or payload.get("profile") != asdict(profile)
        or payload.get("environment") != _include_environment(target=True)
    ):
        raise ProfileError(f"Outdated ESP profile {profile.name}; provision again")
    environment = ProfileEnvironment(
        profile,
        directory,
        Path(str(payload["compiler"])),
        str(payload["compiler_version"]),
        tuple(cast(list[str], payload["arguments"])),
        tuple(Path(path) for path in cast(list[str], payload["include_paths"])),
        tuple(Path(path) for path in cast(list[str], payload["identity_files"])),
        tuple(cast(list[str], payload["removed_arguments"])),
        tuple(
            Path(path) for path in cast(list[str], payload.get("dependency_roots", []))
        ),
    )
    if f"-I{PROJECT_ROOT / 'src'}" not in environment.arguments:
        raise ProfileError(
            f"ESP profile {profile.name} belongs to another checkout; provision again"
        )
    _validate_environment(environment)
    return environment


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--provision", action="store_true")
    parser.add_argument(
        "--profile", choices=[profile.name for profile in ESP_PROFILES], action="append"
    )
    args = parser.parse_args()
    try:
        query = provision_target_query() if args.provision else load_target_query()
        host = provision_host_profile() if args.provision else load_host_profile()
    except ProfileError as error:
        print(error)
        return 1
    print(f"Target query: {query.executable}")
    print(f"Host query: {host.query[0]}; {len(host.include_paths)} include roots")
    for profile in ESP_PROFILES:
        if args.profile and profile.name not in args.profile:
            continue
        try:
            environment = (
                provision_profile(profile) if args.provision else load_profile(profile)
            )
        except ProfileError as error:
            print(error)
            return 1
        print(
            f"{profile.name}: {profile.target}, IDF {profile.idf_major}; {len(environment.include_paths)} include roots"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
