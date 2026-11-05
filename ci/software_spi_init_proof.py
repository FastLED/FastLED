"""Actual isolated #5057 host32 execution; never accepts external proof claims.

Reports describe a single host model. They are not audit-slot authorization or
vendor/other-width evidence. Failed attempts and restored snapshots are retained.
"""

import configparser
import json
import os
import re
import shlex
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path
from tempfile import mkdtemp

from running_process import (
    CalledProcessError,
    CompletedProcess,
    RunningProcess,
    TimeoutExpired,
)

from ci import software_spi_audit as audit


SOURCE = "src/platforms/shared/spi_bitbang/generic_software_spi.h"
TEST = "tests/platforms/shared/spi_bitbang/generic_software_spi.cpp"
CASE = "Software SPI production init establishes low clock before first byte"
# Reuse the production managed Meson setup/compile APIs, without running the
# entire target (mutants intentionally fail). No caller supplies code or target.
BUILD_SCRIPT = """
from pathlib import Path
from ci.meson.build_config import setup_meson_build
from ci.meson.compile import compile_meson
import sys
root = Path.cwd()
build = root / '.build' / 'meson-debug'
if sys.argv[1] == 'runner':
    if not setup_meson_build(root, build, debug=True, build_mode='debug',
                             verbose=True, enable_examples=False,
                             enable_full_examples=False):
        raise SystemExit(2)
result = compile_meson(build, target=sys.argv[1], verbose=True, build_mode='debug',
                       verbose_commands=True)
raise SystemExit(0 if result.success else 2)
"""


@dataclass(frozen=True)
class ToolIdentity:
    path: Path
    sha256: str
    version: str


@dataclass(frozen=True)
class InitStage:
    name: str
    source_sha256: str
    inputs_before: dict[str, str]
    inputs_after: dict[str, str]
    tools_before: dict[str, ToolIdentity]
    tools_after: dict[str, ToolIdentity]
    build_returncode: int
    rebuilt_source: str
    binary_path: Path
    binary_sha256: str
    build_log: Path
    build_log_sha256: str
    runtime_log: Path
    runtime_log_sha256: str
    runtime_command: list[str]
    runtime_returncode: int
    runtime_dependencies: dict[str, str]
    build_context: "BuildContext"
    executed_commands: tuple[tuple[str, ...], ...]


@dataclass(frozen=True)
class InitProof:
    finding: str
    scope: str
    workspace: Path
    stages: list[InitStage]
    gitlinks: dict[str, "GitlinkInput"]


@dataclass(frozen=True)
class GitlinkInput:
    mode: str
    oid: str
    reason: str


@dataclass(frozen=True)
class SnapshotInputs:
    files: list[str]
    gitlinks: dict[str, GitlinkInput]


@dataclass(frozen=True)
class BuildContext:
    configuration: dict[str, str]
    environment: dict[str, str]
    compile_argv: tuple[str, ...]
    directory: Path


def _managed_environment(workspace: Path) -> dict[str, str]:
    from ci.meson.sequential_runner import _make_direct_test_env

    env = _make_direct_test_env(workspace, workspace / ".build/meson-debug", "debug")
    env.update(
        UV_NO_SYNC="1",
        VIRTUAL_ENV=sys.prefix,
        UV_PROJECT_ENVIRONMENT=sys.prefix,
        PYTHONPATH=str(workspace.resolve()),
    )
    return env


def _command_tokens(command: str) -> tuple[str, ...]:
    # The current adapter has observed POSIX Meson/Ninja command syntax only.
    # Windows quoting requires its own observed execution contract.
    if os.name != "posix":
        raise ValueError("unsupported executed command quoting")
    tokens = tuple(shlex.split(command))
    if not tokens or any(token in {"&&", "||", ";", "|"} for token in tokens):
        raise ValueError("unsupported executed command shell composition")
    return tokens


def _generated_configuration_paths(build: Path) -> list[Path]:
    paths: set[Path] = set()
    for pattern in ("*.ninja", "*.rsp", "*.ini", "*.h", "*.hpp", "*.inc"):
        paths.update(build.rglob(pattern))
    paths.update(build.glob("*.txt"))
    paths.update(
        path
        for path in (build / "meson-private").glob("*")
        if path.suffix in {".dat", ".ini"} or path.name == "cmd_line.txt"
    )
    return sorted(paths)


def _retain_generated_configuration(workspace: Path, destination: Path) -> None:
    build = workspace / ".build/meson-debug"
    destination.mkdir(parents=True, exist_ok=False)
    manifest: dict[str, str] = {}
    paths = _generated_configuration_paths(build)
    paths.extend((build / "meson-info").glob("intro-*.json"))
    paths.append(build / "compile_commands.json")
    for path in sorted(set(paths)):
        if path.is_symlink() or not path.is_file():
            raise ValueError("unsafe generated configuration diagnostic input")
        relative = path.relative_to(build)
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        data = path.read_bytes()
        target.write_bytes(data)
        manifest[str(relative)] = audit.source_digest(data)
    (destination / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True), encoding="utf-8"
    )


def _capture_build_context(
    workspace: Path, environment: dict[str, str]
) -> BuildContext:
    build = workspace / ".build/meson-debug"
    configuration: dict[str, str] = {}
    for name in (
        "intro-buildoptions.json",
        "intro-compilers.json",
        "intro-machines.json",
    ):
        path = build / "meson-info" / name
        data = audit._read_audit_json(path)
        configuration[name] = audit.source_digest(
            json.dumps(data, sort_keys=True, separators=(",", ":")).encode()
        )
        if name == "intro-buildoptions.json":
            options = {row["name"]: row["value"] for row in data}
            if len(options) != len(data):
                raise ValueError("duplicate build configuration options")
            if any(
                options.get(key) != value
                for key, value in {
                    "buildtype": "debug",
                    "debug": True,
                    "optimization": "0",
                }.items()
            ):
                raise ValueError("unexpected debug build configuration")
    rows = audit._read_audit_json(build / "compile_commands.json")
    selected = [
        row
        for row in rows
        if (Path(row["directory"]) / row["file"]).resolve()
        == (workspace / TEST).resolve()
    ]
    if len(selected) != 1:
        raise ValueError("missing or ambiguous selected compilation command")
    row = selected[0]
    directory = Path(row["directory"]).resolve()
    if directory != build.resolve():
        raise ValueError("unexpected compilation command directory")
    argv = (
        tuple(row["arguments"])
        if "arguments" in row
        else _command_tokens(row["command"])
    )
    configuration["compile_commands.json"] = audit.source_digest(
        json.dumps(rows, sort_keys=True, separators=(",", ":")).encode()
    )
    # Only known opaque Meson serialization is diagnostic-only. Full options,
    # compiler/machine metadata, compile DB and expanded Ninja commands remain
    # independently bound; never deserialize vendor pickle data.
    opaque = {"build.dat", "coredata.dat", "meson_test_setup.dat"}
    paths = [
        path
        for path in _generated_configuration_paths(build)
        if not (path.parent == build / "meson-private" and path.name in opaque)
    ]
    for token in argv:
        if token.startswith("@"):
            paths.append(directory / token[1:])
    for path in paths:
        if path.is_symlink() or not path.is_file():
            raise ValueError("unsafe or missing generated configuration input")
        content = path.read_bytes()
        if path.suffix == ".ninja":
            # Comments and empty lines are serialization, not Ninja statements.
            # Preserve every other byte, including indentation/escaped spaces.
            content = b"\n".join(
                line
                for line in content.splitlines()
                if line.strip() and not line.lstrip().startswith(b"#")
            )
        configuration[str(path.resolve())] = audit.source_digest(content)
    return BuildContext(
        configuration,
        {
            key: audit.source_digest(value.encode())
            for key, value in environment.items()
        },
        argv,
        directory,
    )


def _verify_build_context(
    workspace: Path, environment: dict[str, str], initial: BuildContext
) -> None:
    if _capture_build_context(workspace, environment) != initial:
        raise ValueError("init proof build context/environment/recipe changed")


def _observed_commands(output: str) -> tuple[tuple[str, ...], ...]:
    commands = []
    for line in output.splitlines():
        # RunningProcess and the managed compile child can each prepend one
        # numeric TimestampFormatter field. Strip only the observed 0–2 fields
        # immediately followed by Ninja progress, never arbitrary leading text.
        line = re.sub(r"^(?:[0-9]+\.[0-9]+ ){0,2}\[\d+/\d+\]\s*", "", line).strip()
        if "ctc-clang++" in line and not line.startswith(("FAILED:", "Compiling")):
            commands.append(_command_tokens(line))
    return tuple(commands)


def _validate_executed_compile_command(output: str, context: BuildContext) -> None:
    # Meson compile DB omits the injected zccache prefix used by the real Ninja
    # rule. This single known launcher is separately content/tool bound.
    commands = []
    for command in _observed_commands(output):
        if Path(command[0]).name in {"zccache", "zccache.exe"}:
            command = command[1:]
        if command == context.compile_argv:
            commands.append(command)
        elif any(TEST in arg or Path(TEST).name == Path(arg).name for arg in command):
            raise ValueError("executed compilation argv mismatch")
    if len(commands) != 1:
        raise ValueError("missing or ambiguous executed compilation command")


def _planned_commands(
    workspace: Path, environment: dict[str, str]
) -> tuple[tuple[str, ...], ...]:
    ninja = shutil.which("ninja", path=environment.get("PATH"))
    if not ninja:
        raise ValueError("managed Ninja command-plan query unavailable")
    result = RunningProcess.run(
        [
            ninja,
            "-C",
            str(workspace / ".build/meson-debug"),
            "-t",
            "commands",
            "tests/generic_software_spi.so",
        ],
        cwd=workspace,
        env=environment,
        check=True,
        timeout=30,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    commands = _observed_commands(result.stdout)
    if not commands:
        raise ValueError("missing native command plan")
    return commands


def _validate_stage_commands(
    output: str, context: BuildContext, planned: tuple[tuple[str, ...], ...]
) -> tuple[tuple[str, ...], ...]:
    _validate_executed_compile_command(output, context)
    observed = _observed_commands(output)
    if any(command not in planned for command in observed):
        raise ValueError("executed PCH/compile/link command differs from initial plan")
    links = [
        command
        for command in observed
        if "-c" not in command
        and any(arg.endswith("/generic_software_spi.so") for arg in command)
    ]
    if len(links) != 1:
        raise ValueError("missing or ambiguous executed target link command")
    return observed


def _snapshot_inputs(root: Path) -> SnapshotInputs:
    options = dict(
        cwd=root,
        check=True,
        timeout=30,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    indexed = RunningProcess.run(["git", "ls-files", "--stage", "-z"], **options)
    gitlinks: dict[str, GitlinkInput] = {}
    for record in indexed.stdout.split("\0"):
        if not record:
            continue
        metadata, name = record.split("\t", 1)
        mode, oid, stage = metadata.split()
        if mode != "160000":
            continue
        if name != "wiki" or stage != "0" or not re.fullmatch(r"[0-9a-f]{40}", oid):
            raise ValueError(f"unreviewed or native Gitlink input: {name}")
        modules = root / ".gitmodules"
        if not modules.is_file() or modules.is_symlink():
            raise ValueError("documentation Gitlink requires registered .gitmodules")
        config = configparser.ConfigParser(interpolation=None)
        config.read_string(modules.read_text(encoding="utf-8"))
        section = 'submodule "wiki"'
        if not config.has_section(section) or dict(config[section]) != {
            "path": "wiki",
            "url": "https://github.com/fastled/fastled.wiki.git",
        }:
            raise ValueError("unregistered documentation Gitlink: wiki")
        gitlinks[name] = GitlinkInput(
            mode,
            oid,
            "Reviewed registered wiki documentation outside native source/test/CI inputs",
        )
    names = RunningProcess.run(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        **options,
    ).stdout.split("\0")
    return SnapshotInputs(sorted(set(names) - {"", *gitlinks}), gitlinks)


def _snapshot(root: Path) -> tuple[Path, SnapshotInputs]:
    # Validate required real inputs before invoking even Git (unit-tested).
    for name in (SOURCE, TEST, "src/platforms/posix/run_unit_test.hpp", "meson.build"):
        if not (root / name).is_file() or (root / name).is_symlink():
            raise ValueError(f"missing or unsafe init proof input: {name}")
    manifest = _snapshot_inputs(root)
    paths = manifest.files
    # clud-git condition 3: main contains the user's in-progress changes and
    # must never receive the restored-defect source mutations.
    parent = Path.home() / ".clud" / "worktrees"
    parent.mkdir(parents=True, exist_ok=True)
    workspace = Path(
        mkdtemp(prefix="fastled3-wt-spi-init-proof-", dir=parent)
    ).resolve()
    RunningProcess.run(
        ["git", "worktree", "add", "--detach", str(workspace), "HEAD"],
        cwd=root,
        check=True,
        timeout=60,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    for name in paths:
        src = root / name
        if src.is_symlink() or not src.is_file():
            raise ValueError(f"unsafe snapshot input (retained {workspace}): {name}")
        dst = workspace / name
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
    # Record untracked support inputs too; never copy ignored generated caches.
    for offset in range(0, len(paths), 200):
        RunningProcess.run(
            ["git", "add", "-f", "--", *paths[offset : offset + 200]],
            cwd=workspace,
            check=True,
            timeout=30,
            capture_output=True,
            encoding="utf-8",
            errors="strict",
        )
    for name, gitlink in manifest.gitlinks.items():
        RunningProcess.run(
            [
                "git",
                "update-index",
                "--add",
                "--cacheinfo",
                f"{gitlink.mode},{gitlink.oid},{name}",
            ],
            cwd=workspace,
            check=True,
            timeout=30,
            capture_output=True,
            encoding="utf-8",
            errors="strict",
        )
    return workspace, manifest


def _inputs(workspace: Path) -> dict[str, str]:
    inputs = audit._native_input_hashes(workspace, audit._current_sources(workspace))
    manifest = _snapshot_inputs(workspace)
    for name in manifest.files:
        path = workspace / name
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"unsafe or missing immutable input: {name}")
        inputs[name] = audit.source_digest(path.read_bytes())
    for name, gitlink in manifest.gitlinks.items():
        inputs[f"gitlink:{name}"] = f"{gitlink.mode}:{gitlink.oid}"
    if (workspace / ".gitmodules").is_file():
        inputs[".gitmodules"] = audit.source_digest(
            (workspace / ".gitmodules").read_bytes()
        )
    return inputs


def _run(
    command: list[str],
    workspace: Path,
    log: Path,
    *,
    build: bool,
    environment: dict[str, str] | None = None,
) -> CompletedProcess[str]:
    env = _managed_environment(workspace) if environment is None else environment
    # Source snapshot uses the already provisioned environment, without syncing
    # dependencies or changing the original checkout's environment.
    log.parent.mkdir(parents=True, exist_ok=True)
    try:
        result = RunningProcess.run(
            command,
            cwd=workspace,
            env=env,
            check=False,
            timeout=1800 if build else 120,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
        )
    except TimeoutExpired as error:
        log.write_bytes(f"init proof timeout: {error}\n".encode("utf-8"))
        raise ValueError(f"init proof timeout; retained {log}") from error
    log.write_bytes(result.stdout.encode("utf-8"))
    return result


def _tool(path: Path, workspace: Path, *, loader: bool = False) -> ToolIdentity:
    path = path.resolve(strict=True)
    before = audit.source_digest(path.read_bytes())
    if loader:
        # This managed loader has no version CLI. Its exact executable content
        # identity is the version; do not invent a compiler-style version string.
        version = f"managed-test-loader-sha256:{before}"
    else:
        result = RunningProcess.run(
            [str(path), "--version"],
            cwd=workspace,
            check=True,
            timeout=30,
            capture_output=True,
            encoding="utf-8",
            errors="strict",
        )
        version = result.stdout.strip()
        if not version:
            raise ValueError(f"empty native tool identity: {path}")
    if audit.source_digest(path.read_bytes()) != before:
        raise ValueError(f"native tool changed while identifying: {path}")
    return ToolIdentity(path, before, version)


def _tools(workspace: Path) -> dict[str, ToolIdentity]:
    from ci.meson.native_launchers import (
        _find_zccache_binary,
        _resolve_fast_compiler_binary,
    )
    from ci.tools.reld import bridge_linker

    build = workspace / ".build/meson-debug"
    info = json.loads((build / "meson-info/intro-compilers.json").read_text())
    launcher = info["host"]["cpp"]["exelist"]
    if not isinstance(launcher, list) or len(launcher) != 1:
        raise ValueError("unsupported native compiler launcher identity")
    options = json.loads((build / "meson-info/intro-buildoptions.json").read_text())
    linker = next(row["value"] for row in options if row["name"] == "native_linker")
    compiler = _resolve_fast_compiler_binary()
    cache = _find_zccache_binary()
    if not compiler or not cache:
        raise ValueError("managed native compiler/cache identity unavailable")
    paths = {
        "compiler": Path(compiler),
        "compiler-launcher": Path(launcher[0]),
        "linker": Path(linker),
        "bridge-linker": Path(bridge_linker()),
        "cache-launcher": Path(cache),
        "python": Path(sys.executable),
        "ninja": Path(shutil.which("ninja") or "missing-managed-ninja"),
        "loader": build / "tests" / ("runner.exe" if os.name == "nt" else "runner"),
    }
    return {
        name: _tool(path, workspace, loader=name == "loader")
        for name, path in paths.items()
    }


def _verify_inputs(workspace: Path, expected: dict[str, str]) -> dict[str, str]:
    actual = _inputs(workspace)
    if actual != expected:
        raise ValueError(f"init proof inputs changed (retained {workspace})")
    return actual


def _stage(
    workspace: Path,
    artifacts: Path,
    name: str,
    source: bytes,
    initial: dict[str, str],
    initial_tools: dict[str, ToolIdentity],
    context: BuildContext,
    environment: dict[str, str],
    planned: tuple[tuple[str, ...], ...],
) -> InitStage:
    _verify_build_context(workspace, _managed_environment(workspace), context)
    if _planned_commands(workspace, environment) != planned:
        raise ValueError("init proof command plan changed")
    target = workspace / SOURCE
    target.write_bytes(source)
    # Force the actual production-template translation unit through compilation;
    # log and rebuilt artifact checks below reject stale/no-op build execution.
    os.utime(workspace / TEST, None)
    before = dict(initial)
    before[SOURCE] = audit.source_digest(source)
    _verify_inputs(workspace, before)
    tools_before = _tools(workspace)
    if tools_before != initial_tools:
        raise ValueError(f"init proof tool identity changed; retained {workspace}")
    saved = artifacts / name
    build_log = saved / "build.log"
    build = workspace / ".build/meson-debug"
    candidates = [
        build / "tests" / f"generic_software_spi{ext}"
        for ext in (".so", ".dylib", ".dll")
    ]
    saved.mkdir(parents=True, exist_ok=True)
    (saved / "build-context.json").write_text(
        json.dumps(
            {
                "configuration": context.configuration,
                "environment_sha256": context.environment,
                "compile_argv": context.compile_argv,
                "directory": str(context.directory),
                "planned_commands": planned,
            },
            indent=2,
            sort_keys=True,
        ),
        encoding="utf-8",
    )
    _retain_generated_configuration(workspace, saved / "generated-before")
    for previous in candidates:
        if previous.exists():
            previous.rename(saved / ("previous-" + previous.name))
    try:
        result = _run(
            [sys.executable, "-c", BUILD_SCRIPT, "generic_software_spi"],
            workspace,
            build_log,
            build=True,
            environment=environment,
        )
    finally:
        _retain_generated_configuration(workspace, saved / "generated-after")
    if result.returncode != 0:
        raise ValueError(f"init proof compile failed; retained {build_log}")
    binary = next((p for p in candidates if p.is_file()), None)
    if binary is None:
        raise ValueError(f"init proof missing or stale binary; retained {build_log}")
    if (
        "generic_software_spi.cpp" not in result.stdout
        or "Compiling" not in result.stdout
    ):
        raise ValueError(
            f"init proof missing actual template compilation; retained {build_log}"
        )
    _verify_build_context(workspace, _managed_environment(workspace), context)
    executed = _validate_stage_commands(result.stdout, context, planned)
    if _planned_commands(workspace, environment) != planned:
        raise ValueError("init proof command plan changed during build")
    saved_binary = saved / binary.name
    shutil.copy2(binary, saved_binary)
    command = [
        str(tools_before["loader"].path),
        str(saved_binary),
        f"--test-case={CASE}",
        "--success",
        "--no-colors",
    ]
    binary_hash = audit.source_digest(saved_binary.read_bytes())
    dependency_paths = sorted(
        path
        for path in (build / "ci/meson/native").iterdir()
        if path.is_file() and path.suffix in {".so", ".dylib", ".dll"}
    )
    pch = build / "tests/test_pch.h.pch"
    if not pch.is_file():
        raise ValueError("init proof missing actual PCH artifact")
    dependency_paths.append(pch)
    dependencies = {
        str(path): audit.source_digest(path.read_bytes()) for path in dependency_paths
    }
    for path in dependency_paths:
        shutil.copy2(path, saved / path.name)
    runtime_log = saved / "runtime.log"
    runtime = _run(
        command, workspace, runtime_log, build=False, environment=environment
    )
    _verify_build_context(workspace, _managed_environment(workspace), context)
    if dependencies != {
        str(path): audit.source_digest(path.read_bytes()) for path in dependency_paths
    }:
        raise ValueError(f"init proof runtime dependencies changed; retained {saved}")
    after = _verify_inputs(workspace, before)
    tools_after = _tools(workspace)
    if tools_before != tools_after:
        raise ValueError(f"init proof native tools changed; retained {saved}")
    if binary_hash != audit.source_digest(
        saved_binary.read_bytes()
    ) or binary_hash != audit.source_digest(binary.read_bytes()):
        raise ValueError(
            f"init proof binary changed during execution; retained {saved}"
        )
    audit._validate_init_clock_low_output(name, runtime.returncode, runtime.stdout)
    return InitStage(
        name,
        audit.source_digest(source),
        before,
        after,
        tools_before,
        tools_after,
        result.returncode,
        SOURCE,
        saved_binary,
        binary_hash,
        build_log,
        audit.source_digest(build_log.read_bytes()),
        runtime_log,
        audit.source_digest(runtime_log.read_bytes()),
        command,
        runtime.returncode,
        dependencies,
        context,
        executed,
    )


def run_init_clock_low_proof(root: Path) -> InitProof:
    root = root.resolve()
    # Missing-input validation in _snapshot deliberately precedes any process.
    workspace, manifest = _snapshot(root)
    original = (workspace / SOURCE).read_bytes()
    root_before = _inputs(root)
    if _inputs(workspace) != root_before:
        raise ValueError(
            f"source snapshot differs from original inputs; retained {workspace}"
        )
    context: BuildContext | None = None
    try:
        initial = _inputs(workspace)
        artifacts = root / ".build" / "spi-init-proof" / workspace.name
        bootstrap = artifacts / "bootstrap.log"
        environment = _managed_environment(workspace)
        result = _run(
            [sys.executable, "-c", BUILD_SCRIPT, "runner"],
            workspace,
            bootstrap,
            build=True,
            environment=environment,
        )
        if result.returncode != 0:
            raise ValueError(f"init proof loader build failed; retained {bootstrap}")
        _verify_inputs(workspace, initial)
        initial_tools = _tools(workspace)
        context = _capture_build_context(workspace, environment)
        planned = _planned_commands(workspace, environment)
        stages = {
            "baseline": original,
            **audit._init_clock_low_mutants(original),
            "restored": original,
        }
        records: list[InitStage] = []
        expected = dict(initial)
        for name, source in stages.items():
            # One trust baseline for the whole attempt: never adopt changed
            # oracle/tool state merely because it is stable inside a stage.
            _verify_inputs(workspace, expected)
            _verify_build_context(workspace, _managed_environment(workspace), context)
            if _planned_commands(workspace, environment) != planned:
                raise ValueError("init proof command plan changed between stages")
            if _tools(workspace) != initial_tools:
                raise ValueError(
                    f"init proof tool identity changed; retained {workspace}"
                )
            records.append(
                _stage(
                    workspace,
                    artifacts,
                    name,
                    source,
                    initial,
                    initial_tools,
                    context,
                    environment,
                    planned,
                )
            )
            expected[SOURCE] = audit.source_digest(source)
            _verify_inputs(workspace, expected)
            _verify_build_context(workspace, _managed_environment(workspace), context)
            if _planned_commands(workspace, environment) != planned:
                raise ValueError("init proof command plan changed between stages")
            if _tools(workspace) != initial_tools:
                raise ValueError(
                    f"init proof tool identity changed; retained {workspace}"
                )
        _verify_inputs(workspace, initial)
        _verify_build_context(workspace, _managed_environment(workspace), context)
        if len({stage.binary_sha256 for stage in records[:3]}) != 3:
            raise ValueError(
                f"init proof mutant binaries are not distinct; retained {workspace}"
            )
        return InitProof(
            "generic-init-clock-low",
            "host32-fixture4-only",
            workspace,
            records,
            manifest.gitlinks,
        )
    except (OSError, ValueError, CalledProcessError, TimeoutExpired) as error:
        raise ValueError(f"init proof failed; retained {workspace}: {error}") from error
    finally:
        try:
            (workspace / SOURCE).write_bytes(original)
            if (workspace / SOURCE).read_bytes() != original:
                raise ValueError("init source restoration byte mismatch")
            _verify_inputs(workspace, root_before)
            _verify_inputs(root, root_before)
            if context is not None:
                _verify_build_context(
                    workspace, _managed_environment(workspace), context
                )
        except (OSError, ValueError) as error:
            raise ValueError(
                f"init proof restoration/input verification failed; retained {workspace}: {error}"
            ) from error
