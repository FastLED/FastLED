"""Closed C6 selection attempt infrastructure, not GPIO execution evidence.

Expected compiler rejection is a typed negative result, never a successful
build or a fresh ELF/database. No caller-provided commands or receipts exist.
"""

import configparser
import hashlib
import json
import re
import shlex
import sys
from dataclasses import asdict, dataclass
from enum import Enum
from pathlib import Path

from ci import software_spi_esp32_selection_proof as selection
from ci import software_spi_init_proof as native
from ci import software_spi_my9221_rp2040_runner as vendor


STAGES = (
    "native-inverse-control",
    "baseline",
    "unguarded-mutant-arduino-first",
    "unguarded-mutant-fastled-first",
    "restored",
)
EXAMPLE = "examples/AutoResearch/AutoResearch.ino"
HOOK = "examples/AutoResearch/AutoResearchStartupHook.cpp"
_REQUIRED_SOURCES = {selection.SOURCE, EXAMPLE, HOOK}


class _SelectionProfile(Enum):
    C6 = "esp32c6"
    XTENSA = "esp32dev"


@dataclass(frozen=True, slots=True)
class _ProfileSpec:
    env: str
    board: str
    framework_version: str
    platform_pin: str | None
    snapshot_kind: native._ProofKind
    artifact_group: str
    scope: str


def _profile_spec(kind: _SelectionProfile) -> _ProfileSpec:
    if kind is _SelectionProfile.C6:
        return _ProfileSpec(
            "esp32c6",
            "ESP32_C6_DEVKITC_1",
            "3.3.5",
            None,
            native._ProofKind.ESP32_C6_SELECTION,
            "spi-esp32-c6-selection-proof",
            "esp32c6-forced1-NoMap-absent-native-inverse-idle900-compiler-selection-property",
        )
    if kind is _SelectionProfile.XTENSA:
        return _ProfileSpec(
            "esp32dev",
            "ESP32DEV",
            "3.3.11",
            "https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip",
            native._ProofKind.ESP32_XTENSA_SELECTION,
            "spi-esp32-xtensa-selection-proof",
            "esp32-xtensa-forced1-NoMap-absent-native-inverse-idle900-compiler-selection-property",
        )
    raise ValueError("unknown closed selection profile")


def _validate_info_profile(info: dict, kind: _SelectionProfile) -> None:
    spec = _profile_spec(kind)
    if info.get("env") != spec.env:
        raise ValueError("selected environment differs from closed profile")
    if kind is _SelectionProfile.C6:
        selection.validate_c6_profile(info)
    else:
        selection.validate_xtensa_profile(info)


def _selection_sdk_roots(info: dict, kind: _SelectionProfile) -> tuple[Path, Path]:
    _validate_info_profile(info, kind)
    # Existing audited root discovery uses actual planned framework paths and
    # exact core/toolchain namespaces for each environment, never a caller root.
    return vendor._profile_roots(info)


@dataclass(frozen=True, slots=True)
class _ProviderCohortPins:
    version: str
    source_commit: str
    cli_sha256: str
    daemon_sha256: str
    extension_sha256: str


_PROVIDER_COHORT_PINS = _ProviderCohortPins(
    "2.5.52",
    "fb63b020fa751017cef2e7dee93be0e600877967",
    "72970a264b551a84c3f3a5e67ad3c0984053959126c7228471c61b6eb01d6e1d",
    "00a47aeaf9b1f2c2119d0e08e966525fba961d8874a163acbdedc56be2eb4d36",
    "78de1c3251f0676e1d27871ca7dfb18439ea3224d15a9dd4d72e44ca36368b01",
)


@dataclass(frozen=True, slots=True)
class _ProviderObservation:
    prefix: Path
    python: Path
    package_root: Path
    cli: Path
    daemon: Path
    extension: Path
    package_version: str


@dataclass(frozen=True, slots=True)
class _ProviderCohort:
    version: str
    source_commit: str
    component_sha256s: dict[str, str]
    python_sha256: str
    paths: dict[str, str]


def _observe_provider_cohort() -> _ProviderObservation:
    from importlib import metadata, util

    distribution = metadata.distribution("fbuild")
    spec = util.find_spec("fbuild._native")
    if spec is None or not spec.origin:
        raise ValueError("missing selected provider extension")
    prefix = Path(sys.prefix).absolute()
    return _ProviderObservation(
        prefix=prefix,
        python=Path(sys.executable).absolute(),
        package_root=Path(distribution.locate_file("fbuild")).absolute(),
        cli=prefix / "bin/fbuild",
        daemon=prefix / "bin/fbuild-daemon",
        extension=Path(spec.origin).absolute(),
        package_version=distribution.version,
    )


def _verify_provider_cohort() -> _ProviderCohort:
    # This Linux managed-proof cohort is NOT a global board dependency floor.
    # Exact images are tied to the retained, independently verified fb63 wheel;
    # neither an advertised version nor a sister-checkout HEAD substitutes.
    try:
        observed = _observe_provider_cohort()
        pins = _PROVIDER_COHORT_PINS
        prefix = observed.prefix.absolute()
        package = observed.package_root.absolute()
        if (
            observed.package_version != pins.version
            or observed.python.absolute().parent != prefix / "bin"
            or observed.cli.absolute() != prefix / "bin/fbuild"
            or observed.daemon.absolute() != prefix / "bin/fbuild-daemon"
            or package.name != "fbuild"
            or not package.resolve().is_relative_to(prefix.resolve())
            or observed.extension.absolute().parent != package
            or observed.extension.name != "_native.abi3.so"
            or not observed.extension.resolve().is_relative_to(prefix.resolve())
        ):
            raise ValueError("foreign selected provider/version context")
        paths = {
            "fbuild": observed.cli,
            "fbuild-daemon": observed.daemon,
            "_native.abi3.so": observed.extension,
        }
        expected = {
            "fbuild": pins.cli_sha256,
            "fbuild-daemon": pins.daemon_sha256,
            "_native.abi3.so": pins.extension_sha256,
        }
        actual = {}
        for name, path in paths.items():
            if path.is_symlink() or not path.is_file():
                raise ValueError("missing or aliased provider component " + name)
            actual[name] = vendor._digest(path)
            if actual[name] != expected[name]:
                raise ValueError("provider cohort hash mismatch for " + name)
        if not observed.python.is_file():
            raise ValueError("missing selected provider interpreter")
        return _ProviderCohort(
            pins.version,
            pins.source_commit,
            actual,
            vendor._digest(observed.python),
            {
                "prefix": str(prefix),
                "python": str(observed.python),
                "package_root": str(package),
                **{name: str(path) for name, path in paths.items()},
            },
        )
    except (OSError, KeyError, TypeError, ImportError) as error:
        raise ValueError("provider cohort missing or unbound") from error


@dataclass(frozen=True)
class SelectionOutcome:
    returncode: int
    expected_compile_failure: bool
    fresh_elf_eligible: bool
    fresh_planned_database_eligible: bool
    runtime_gpio_proven: bool = False
    complete_executed_argv: bool = False


@dataclass(frozen=True)
class SelectionStage:
    name: str
    project: Path
    outcome: SelectionOutcome
    log_path: Path
    log_sha256: str
    inputs_before: dict
    inputs_after: dict
    staged_sources_expected: dict
    staged_sources_observed: dict
    tools_before: dict
    tools_after: dict
    profile_before: dict
    profile_after: dict
    serving_engine_before: dict
    serving_engine_after: dict
    artifact_sha256s: dict[str, str]
    configuration_sha256s: dict[str, str]


@dataclass(frozen=True)
class SelectionBuildSession:
    compile_result: object
    before: dict
    after: dict
    static_after: dict


def stage_defines(name: str) -> list[str]:
    if name not in STAGES:
        raise ValueError("unknown fixed selection stage")
    scope = 1 if name == STAGES[2] else 2 if name == STAGES[3] else 0
    flags = (
        ["-DEXPECT_FORCED=0", "-DEXPECT_NO_PINMAP=0"]
        if name == STAGES[0]
        else [
            "-DEXPECT_FORCED=1",
            "-DEXPECT_NO_PINMAP=0",
            "-DFASTLED_FORCE_SOFTWARE_PINS=1",
        ]
    )
    return [*flags, f"-DFASTLED_SELECTION_ASSERT_SCOPE={scope}"]


def validate_stage_defines(name: str, tokens: list[str]) -> None:
    if name not in STAGES:
        raise ValueError("unknown fixed selection stage")
    selection.validate_global_defines(tokens, None if name == STAGES[0] else 1)
    key = "FASTLED_SELECTION_ASSERT_SCOPE"
    expected = "1" if name == STAGES[2] else "2" if name == STAGES[3] else "0"
    definitions = []
    index = 0
    while index < len(tokens):
        token = tokens[index]
        index += 1
        if token.startswith("-Wp,"):
            for part in token[4:].split(","):
                operand = part[2:] if part.startswith(("-D", "-U")) else part
                match = re.match(r"[A-Za-z_]\w*", operand)
                if match and match[0] == key:
                    raise ValueError("forwarded assertion-scope override")
        operation, operand = token[:2], token[2:]
        if operation not in ("-D", "-U"):
            continue
        if not operand:
            if index == len(tokens):
                raise ValueError("missing assertion-scope operand")
            operand = tokens[index]
            index += 1
        match = re.match(r"[A-Za-z_]\w*", operand)
        if match and match[0] == key:
            if (
                operation == "-U"
                or re.fullmatch(r"[A-Za-z_]\w*(?:=[^\s=]+)?", operand) is None
            ):
                raise ValueError("non-scalar or undefined assertion scope")
            definitions.append(operand.partition("=")[2] if "=" in operand else "1")
    if definitions != [expected]:
        raise ValueError("missing, conflicting or duplicate assertion scope")


def producer_sources(root: Path) -> dict[str, bytes]:
    base = root / "ci/fixtures/software_spi_audit"
    return {
        EXAMPLE: (base / "esp32_c6_selection_runner_producer.cpp.txt").read_bytes(),
        HOOK: (base / "esp32_c6_selection_runner_startup.cpp.txt").read_bytes(),
    }


def source_for_stage(original: bytes, name: str) -> bytes:
    if name not in STAGES:
        raise ValueError("unknown fixed selection stage")
    return (
        selection.unguarded_native_mutant(original) if name in STAGES[2:4] else original
    )


def validate_stage_sequence(stages: tuple[str, ...]) -> None:
    if stages != STAGES:
        raise ValueError("incomplete or reordered fixed selection stages")


def _scoped_diagnostics(name: str) -> set[tuple[str, int, str]]:
    arduino = name == STAGES[2]
    if name not in STAGES[2:4]:
        raise ValueError("not an assertion-scoped negative")
    filename = (
        "esp32_c6_selection_runner_producer.cpp.txt"
        if arduino
        else "esp32_c6_selection_runner_startup.cpp.txt"
    )
    source = (
        "src/sketch/AutoResearch.ino"
        if arduino
        else "src/sketch/AutoResearchStartupHook.cpp"
    )
    lines = (
        (
            Path(__file__).resolve().parents[1]
            / "ci/fixtures/software_spi_audit"
            / filename
        )
        .read_text()
        .splitlines()
    )
    messages = (
        '#error "forced selection must not advertise hardware pin support"',
        "static assertion failed: forced GPIO2 must use ForcedSoftwarePin",
        "static assertion failed: forced GPIO3 must use ForcedSoftwarePin",
    )
    result = set()
    for message in messages:
        token = (
            "must not advertise hardware pin support"
            if message.startswith("#error")
            else message.removeprefix("static assertion failed: ")
        )
        matches = [index for index, line in enumerate(lines, 1) if token in line]
        if len(matches) != 1:
            raise ValueError("unbound scoped diagnostic source")
        result.add((source, matches[0], message))
    return result


def assess_compile_result(
    name: str, returncode: int, output: str, project: Path
) -> SelectionOutcome:
    if name not in STAGES or type(returncode) is not int or not isinstance(output, str):
        raise ValueError("unknown fixed selection result")
    negative = name in STAGES[2:4]
    if not negative:
        selection.validate_stage_output("baseline", returncode, output)
    else:
        if returncode != 1:
            raise ValueError("scoped mutant must be an actual rejected compile")
        expected = _scoped_diagnostics(name)
        target = project / (
            "src/main.cpp"
            if name == STAGES[2]
            else "src/sketch/AutoResearchStartupHook.cpp"
        )
        envelope = f"build error: build failed: compilation failed for {target}:"
        summaries = 0
        observed = set()
        for line in output.splitlines():
            if line == envelope:
                summaries += 1
                continue
            if not re.search(
                r"\b(?:fatal\s+)?error:|AddressSanitizer|UndefinedBehaviorSanitizer",
                line,
            ):
                continue
            match = re.fullmatch(
                r"(src/sketch/(?:AutoResearch\.ino|AutoResearchStartupHook\.cpp)):(\d+):(\d+): error: (.+)",
                line,
            )
            if match is None or int(match[3]) < 1:
                raise ValueError("unbound compiler envelope or diagnostic")
            diagnostic = (match[1], int(match[2]), match[4])
            if diagnostic not in expected:
                raise ValueError("diagnostic outside the selected TU/assertion scope")
            observed.add(diagnostic)
        if not summaries or observed != expected:
            raise ValueError("missing bound wrapper or designated scoped triple")
    return SelectionOutcome(returncode, negative, not negative, not negative)


def _verify_files(hashes: dict[str, str]) -> None:
    for name, sha in hashes.items():
        path = Path(name)
        if not path.is_file() or path.is_symlink() or vendor._digest(path) != sha:
            raise ValueError("retained selection artifact/configuration changed")


def validate_stage_record(stage: SelectionStage) -> None:
    if not isinstance(stage, SelectionStage) or stage.name not in STAGES:
        raise ValueError("unbound typed selection stage")
    for before, after in (
        (stage.inputs_before, stage.inputs_after),
        (stage.staged_sources_expected, stage.staged_sources_observed),
        (stage.tools_before, stage.tools_after),
        (stage.profile_before, stage.profile_after),
        (stage.serving_engine_before, stage.serving_engine_after),
    ):
        if not before or before != after:
            raise ValueError("missing or changed selection-stage binding")
    if stage.staged_sources_expected != _staged_subset(stage.inputs_before):
        raise ValueError("staged receipt is not the actual source/example subset")
    if (
        not _REQUIRED_SOURCES <= stage.inputs_before.keys()
        or not _REQUIRED_SOURCES <= stage.staged_sources_expected.keys()
        or "compiler" not in stage.tools_before
        or not {"pid", "image_sha256"} <= stage.serving_engine_before.keys()
    ):
        raise ValueError("missing mandatory selection identity")
    if (
        not stage.log_path.is_file()
        or stage.log_path.is_symlink()
        or vendor._digest(stage.log_path) != stage.log_sha256
    ):
        raise ValueError("selection compiler log changed")
    expected = assess_compile_result(
        stage.name, stage.outcome.returncode, stage.log_path.read_text(), stage.project
    )
    if stage.outcome != expected:
        raise ValueError("inconsistent selection outcome or freshness claim")
    if not stage.configuration_sha256s:
        raise ValueError("missing selection configuration bindings")
    if stage.outcome.expected_compile_failure:
        if any(
            Path(path).name
            in {"firmware.elf", "firmware.bin", "compile_commands.raw.json"}
            for path in stage.artifact_sha256s
        ):
            raise ValueError("failed compilation cannot qualify fresh build artifacts")
    elif not {"firmware.elf", "firmware.bin", "compile_commands.raw.json"} <= {
        Path(path).name for path in stage.artifact_sha256s
    }:
        raise ValueError("eligible positive lacks actual retained artifacts")
    _verify_files(stage.artifact_sha256s)
    _verify_files(stage.configuration_sha256s)


def _selection_environment(workspace: Path) -> dict[str, str]:
    # Explicit nondefault condition for this proof only. fb63 context.rs reads
    # this process variable in the live maintenance loop (entry/mod.rs528/531).
    # Health/status traffic is not work and does not renew an idle lease.
    environment = vendor._environment(workspace)
    environment["FBUILD_SELF_EVICTION_SECS"] = "900"
    return environment


def _run_fixed_compile(
    workspace: Path, saved: Path, environment: dict, name: str, kind: _SelectionProfile
):
    spec = _profile_spec(kind)
    saved.mkdir(parents=True, exist_ok=True)
    command = [
        "bash",
        "compile",
        spec.env,
        "--examples",
        "AutoResearch",
        "--defines",
        ",".join(flag.removeprefix("-D") for flag in stage_defines(name)),
        "-v",
        "--build-info",
        str(saved / "build-info.json"),
    ]
    vendor._json(
        saved / "invocation.json",
        {"argv": command, "cwd": str(workspace), "complete_executed_argv": False},
    )
    # In particular returncode=1 remains the actual rejected process status.
    return native._run(
        command, workspace, saved / "build.log", build=True, environment=environment
    )


def _run_c6_compile(workspace: Path, saved: Path, environment: dict, name: str):
    return _run_fixed_compile(workspace, saved, environment, name, _SelectionProfile.C6)


def _run_xtensa_compile(workspace: Path, saved: Path, environment: dict, name: str):
    return _run_fixed_compile(
        workspace, saved, environment, name, _SelectionProfile.XTENSA
    )


@dataclass(frozen=True, slots=True)
class _CompileWindow:
    compile_result: object
    before: dict
    after: dict


def _verify_submission_route(
    workspace: Path,
    environment: dict,
    output: str,
    kind: _SelectionProfile = _SelectionProfile.C6,
) -> None:
    spec = _profile_spec(kind)
    if not isinstance(environment.get("FBUILD_EXECUTABLE"), str):
        raise ValueError("missing bound submission client")
    command = [
        str(Path(environment["FBUILD_EXECUTABLE"]).resolve()),
        str(workspace / ".build/fbuild" / spec.env),
        "build",
        "-e",
        spec.env,
        "-v",
    ]
    expected = "Running: " + " ".join(command)
    routes = [line for line in output.splitlines() if line.startswith("Running: ")]
    if routes != [expected]:
        raise ValueError("unbound actual C6 selection submission route")


def _compile_engine_window(
    workspace: Path,
    saved: Path,
    environment: dict,
    tools: dict,
    engine_profile: dict,
    name: str,
    kind: _SelectionProfile = _SelectionProfile.C6,
) -> _CompileWindow:
    vendor._ensure_stage_engine(workspace, saved, environment, tools, engine_profile)
    before = vendor._daemon_identity(
        workspace, saved, "build-before", environment, tools
    )
    vendor._verify_engine_profile(saved, "build-before", engine_profile, before)
    after = None
    try:
        completed = (
            _run_c6_compile(workspace, saved, environment, name)
            if kind is _SelectionProfile.C6
            else _run_xtensa_compile(workspace, saved, environment, name)
        )
    finally:
        pending = sys.exc_info()[1]
        try:
            # No hashing, assessment or metadata parsing before this capture.
            after = vendor._daemon_identity(
                workspace, saved, "build-after", environment, tools
            )
            vendor._verify_serving_engine(saved, "build-after", before, after)
        except (OSError, ValueError, KeyError, TypeError) as error:
            failure = {
                "complete": False,
                "compile_error": None
                if pending is None
                else {"type": type(pending).__name__, "message": str(pending)},
                "after_engine_error": {
                    "type": type(error).__name__,
                    "message": str(error),
                },
            }
            if isinstance(pending, KeyboardInterrupt):
                try:
                    vendor._json(saved / "build-session-failure.json", failure)
                finally:
                    # A diagnostic write failure must not swallow user cancellation.
                    raise pending from error
            vendor._json(saved / "build-session-failure.json", failure)
            if pending is not None:
                raise pending from error
            raise
    _verify_submission_route(workspace, environment, completed.stdout, kind)
    return _CompileWindow(completed, before, after)


def _selection_build_session(
    workspace: Path,
    saved: Path,
    environment: dict,
    tools: dict,
    roots: tuple,
    profile: dict,
    engine_profile: dict,
    info: dict,
    expected_inputs: dict,
    name: str,
    kind: _SelectionProfile = _SelectionProfile.C6,
) -> SelectionBuildSession:
    project = workspace / ".build/fbuild" / _profile_spec(kind).env
    before_static = _selection_state(
        info, environment, tools, roots, project, name, kind
    )
    vendor.property_check._verify_profile_state(saved, "before", profile, before_static)
    window = _compile_engine_window(
        workspace, saved, environment, tools, engine_profile, name, kind
    )
    after_static = _selection_state(
        info, environment, tools, roots, project, name, kind
    )
    vendor.property_check._verify_profile_state(
        saved, "after-build", profile, after_static
    )
    native._verify_inputs(workspace, expected_inputs)
    vendor._check_staged_sources(
        workspace,
        expected_inputs,
        project,
        producer_sources(Path(__file__).resolve().parents[1])[EXAMPLE],
    )
    return SelectionBuildSession(
        window.compile_result, window.before, window.after, after_static
    )


@dataclass(frozen=True)
class CellConfiguration:
    name: str
    hashes: dict[str, str]
    contents: dict[str, bytes]
    normalized_recipe: dict


def _normalize_cell_tokens(
    tokens: list[str], name: str, require_defines: bool = True
) -> list[str]:
    force = None if name == STAGES[0] else 1
    if require_defines:
        validate_stage_defines(name, tokens)
    else:
        # A partial argv/rsp still obeys the exact scalar/override grammar.
        # Supply only absent fixed definitions for validation; the expanded
        # complete argv below checks duplicates across argv and all responses.
        present = set()
        for index, token in enumerate(tokens):
            if not isinstance(token, str):
                raise ValueError("nonstring partial recipe token")
            if token.startswith("-D"):
                operand = token[2:]
                if token == "-D" and index + 1 < len(tokens):
                    operand = tokens[index + 1]
                match = re.match(r"[A-Za-z_]\w*", operand)
                if match:
                    present.add(match[0])
        missing = [
            flag
            for flag in stage_defines(name)
            if flag[2:].partition("=")[0] not in present
        ]
        validate_stage_defines(name, [*tokens, *missing])
    result = []
    index = 0
    while index < len(tokens):
        token = tokens[index]
        index += 1
        if not isinstance(token, str):
            raise ValueError("nonstring recipe token")
        operand = token[2:] if token.startswith("-D") else None
        consumed = [token]
        if token == "-D":
            if index == len(tokens):
                raise ValueError("missing macro operand")
            operand = tokens[index]
            consumed.append(operand)
            index += 1
        if operand is not None:
            key, separator, value = operand.partition("=")
            if key in {
                "EXPECT_FORCED",
                "FASTLED_FORCE_SOFTWARE_PINS",
                "FASTLED_SELECTION_ASSERT_SCOPE",
            }:
                expected = (
                    str(int(force is not None))
                    if key == "EXPECT_FORCED"
                    else (
                        "1" if name == STAGES[2] else "2" if name == STAGES[3] else "0"
                    )
                    if key == "FASTLED_SELECTION_ASSERT_SCOPE"
                    else None
                    if force is None
                    else "1"
                )
                if expected is None or (value if separator else "1") != expected:
                    raise ValueError("unexpected cell macro operand")
                continue
        result.extend(consumed)
    return result


def _capture_cell_configuration(
    info: dict, name: str, kind: _SelectionProfile = _SelectionProfile.C6
) -> CellConfiguration:
    spec = _profile_spec(kind)
    if name not in STAGES or info.get("env") != spec.env:
        raise ValueError("not a fixed C6 selection cell")
    project = Path(info["prog_path"]).parents[3]
    ini = project / "platformio.ini"
    parser = configparser.ConfigParser(
        interpolation=None, inline_comment_prefixes=("#", ";"), strict=True
    )
    parser.read_string(ini.read_text())
    section = "env:" + spec.env
    if parser.defaults() or not parser.has_section(section):
        raise ValueError("unreviewed inherited selection configuration")
    if kind is _SelectionProfile.XTENSA:
        if parser.get(section, "platform", raw=True) != spec.platform_pin:
            raise ValueError("unsupported classic selected platform pin")
        _selection_sdk_roots(info, kind)
    normalized_ini = {
        section: dict(parser.items(section, raw=True)) for section in parser.sections()
    }
    flags = shlex.split(parser.get(section, "build_flags", raw=True), comments=True)
    normalized_ini[section]["build_flags"] = _normalize_cell_tokens(flags, name)
    database = Path(info["prog_path"]).parent / "compile_commands.raw.json"
    files = {
        ini,
        database,
        *project.rglob("*.rsp"),
        *project.rglob("compile_commands.raw.json"),
    }

    def expand_responses(
        tokens: list[str], directory: Path, active: frozenset[Path] = frozenset()
    ) -> list[str]:
        if not isinstance(tokens, list) or any(
            not isinstance(token, str) for token in tokens
        ):
            raise ValueError("nonstring planned recipe")
        expanded = []
        for token in tokens:
            if not token.startswith("@"):
                expanded.append(token)
                continue
            candidate = directory / token[1:]
            response = candidate.resolve(strict=True)
            if not response.is_relative_to(project.resolve()) or response in active:
                raise ValueError("external or recursive response file")
            if candidate.is_symlink() or not response.is_file():
                raise ValueError("unsafe response file")
            relative = candidate.absolute().relative_to(project.absolute())
            if any(
                (project / Path(*relative.parts[:index])).is_symlink()
                for index in range(1, len(relative.parts) + 1)
            ):
                raise ValueError("response path traverses symlink")
            files.add(response)
            # GCC14 libiberty/argv.c expandargv rescans inserted arguments
            # without chdir: nested @paths remain relative to compiler CWD.
            expanded.extend(
                expand_responses(
                    shlex.split(response.read_text()),
                    directory,
                    active | {response},
                )
            )
        return expanded

    # Resolve references before snapshotting the configuration universe. Missing
    # paths become a closed validation error, never an uncaptured command input.
    database_rows = {}
    try:
        for path in tuple(files):
            if path.name != "compile_commands.raw.json":
                continue
            rows = json.loads(path.read_text())
            if not isinstance(rows, list) or not rows:
                raise ValueError("missing selection planned rows")
            bound_rows = []
            for row in rows:
                arguments = row.get("arguments")
                command = row.get("command")
                if arguments is None:
                    arguments = shlex.split(command)
                elif command is not None and arguments != shlex.split(command):
                    raise ValueError("contradictory command and arguments")
                directory = Path(row["directory"])
                if not directory.is_absolute():
                    directory = project / directory
                if not directory.resolve().is_relative_to(project.resolve()):
                    raise ValueError("external planned working directory")
                effective = expand_responses(arguments, directory)
                bound_rows.append(
                    {
                        **{
                            key: value
                            for key, value in row.items()
                            if key not in {"command", "arguments"}
                        },
                        "arguments": _normalize_cell_tokens(arguments, name, False),
                        "effective_arguments": _normalize_cell_tokens(effective, name),
                    }
                )
            database_rows[path] = bound_rows
    except (OSError, TypeError, KeyError) as error:
        raise ValueError("unbound planned response/command input") from error
    normalized = {}
    for path in sorted(files):
        if path.is_symlink() or not path.is_file():
            raise ValueError("unsafe selection configuration file")
        relative = path.relative_to(project).as_posix()
        if path.name == "compile_commands.raw.json":
            normalized[relative] = database_rows[path]
        elif path.suffix == ".rsp":
            normalized[relative] = _normalize_cell_tokens(
                shlex.split(path.read_text()), name, False
            )
    contents = {str(path): path.read_bytes() for path in sorted(files)}
    return CellConfiguration(
        name,
        {str(path): vendor._digest(path) for path in sorted(files)},
        contents,
        {"ini": normalized_ini, "recipes": normalized, "prog_path": info["prog_path"]},
    )


def _compare_cell_configurations(
    native_cell: CellConfiguration, forced_cell: CellConfiguration
) -> None:
    if native_cell.name != STAGES[0] or forced_cell.name not in STAGES[1:]:
        raise ValueError("not the two closed selection cells")
    if (
        set(native_cell.hashes) != set(forced_cell.hashes)
        or native_cell.normalized_recipe != forced_cell.normalized_recipe
    ):
        raise ValueError("selection cells differ beyond exact FORCE/EXPECT definitions")


def _selection_state(
    info: dict,
    environment: dict,
    tools: dict,
    roots: tuple,
    project: Path,
    name: str,
    kind: _SelectionProfile = _SelectionProfile.C6,
) -> dict:
    result = vendor._base_state(info, environment, tools, roots, project)
    result["planned_configuration"] = _capture_cell_configuration(
        info, name, kind
    ).hashes
    return result


@dataclass(frozen=True, slots=True)
class SelectionProof:
    workspace: Path
    stages: list[SelectionStage]
    restoration_verified: bool
    idle_policy_seconds: int = 900
    idle_policy_source_commit: str = "fb63b020fa751017cef2e7dee93be0e600877967"
    idle_policy_is_default: bool = False
    idle_policy_is_work_lease: bool = False
    scope: str = "esp32c6-forced1-NoMap-absent-native-inverse-idle900-compiler-selection-property"
    runtime_gpio_proven: bool = False
    complete_executed_argv: bool = False
    full_finding_qualified: bool = False
    limitation: str = "Compiler ancestry/HAS checks only. Explicit 900-second daemon idle condition, not a health lease, unchanged default or V1 cause finding. No GPIO execution, MY support, masks, firmware arrival, cache/timing, classic ESP32, full matrix or full finding qualification."


def validate_attempt(stages: list[SelectionStage], original_source: bytes) -> None:
    validate_stage_sequence(tuple(stage.name for stage in stages))
    first = stages[0]
    unchanged = {
        key: value
        for key, value in first.inputs_before.items()
        if key != selection.SOURCE
    }
    forced_profile = stages[1].profile_before
    for stage in stages:
        validate_stage_record(stage)
        expected_source = (
            selection.unguarded_native_mutant(original_source)
            if stage.name in STAGES[2:4]
            else original_source
        )
        if (
            stage.inputs_before[selection.SOURCE]
            != hashlib.sha256(expected_source).hexdigest()
        ):
            raise ValueError("incorrect controlled source transition")
        if {
            key: value
            for key, value in stage.inputs_before.items()
            if key != selection.SOURCE
        } != unchanged:
            raise ValueError("uncontrolled source/input transition")
        if stage.tools_before != first.tools_before:
            raise ValueError("cross-stage selection tool drift")
        if (
            stage.name in (STAGES[1], STAGES[4])
            and stage.profile_before != forced_profile
        ):
            raise ValueError("forced-cell profile drift")


def _positive_info(
    workspace: Path,
    saved: Path,
    completed,
    kind: _SelectionProfile = _SelectionProfile.C6,
) -> dict:
    if completed.returncode != 0:
        raise ValueError("failed compile cannot supply fresh build-info")
    info = json.loads((saved / "build-info.json").read_text())[_profile_spec(kind).env]
    _validate_info_profile(info, kind)
    _verify_submission_route(
        workspace, _selection_environment(workspace), completed.stdout, kind
    )
    return info


def _retain_cell(saved: Path, cell: CellConfiguration, label: str) -> dict[str, str]:
    result = {}
    for index, (source, content) in enumerate(sorted(cell.contents.items())):
        path = saved / label / f"{index:04d}-{Path(source).name}"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        result[str(path)] = vendor._digest(path)
    vendor._json(
        saved / (label + "-scope.json"),
        {
            "planned_only": True,
            "freshness": "unproven",
            "source_sha256s": cell.hashes,
            "complete_executed_argv": False,
        },
    )
    return result


def _activate_cell(cell: CellConfiguration) -> None:
    for name, data in cell.contents.items():
        path = Path(name)
        if path.is_symlink() or not path.is_file():
            raise ValueError("unsafe known cell configuration restore")
        path.write_bytes(data)
    _verify_files(cell.hashes)


def _compatible_cell_profiles(native_profile: dict, forced_profile: dict) -> None:
    for key in ("tools", "sdk", "environment"):
        if not native_profile.get(key) or native_profile[key] != forced_profile.get(
            key
        ):
            raise ValueError(f"native/forced {key} mismatch")
    # Reported compiler options are immutable. Global user definitions live in
    # the independently parsed INI/planned recipes, compared separately above.
    if native_profile["configuration"] != forced_profile["configuration"]:
        raise ValueError("native/forced reported build context differs")


def _execute_fixed_stages(
    root: Path,
    workspace: Path,
    artifacts: Path,
    initial: dict,
    original_inputs: dict,
    original_source: bytes,
    kind: _SelectionProfile = _SelectionProfile.C6,
) -> list[SelectionStage]:
    environment = _selection_environment(workspace)
    project = workspace / ".build/fbuild" / _profile_spec(kind).env
    clients = vendor._client_tools(environment)
    desired = vendor._observe_launcher(
        workspace, artifacts / "launcher", environment, clients
    )
    bootstrap_engine = vendor._prepare_bootstrap_engine(
        workspace, artifacts / "launcher", environment, clients, desired
    )
    cells = {}
    profiles = {}
    infos = {}
    tools = None
    roots = None
    engine_profile = bootstrap_engine
    # Four cells are explicit uncertified discovery builds: native, forced-both,
    # forced-ino and forced-cpp. None substitutes for a certified stage.
    for name in STAGES[:4]:
        saved = artifacts / ("bootstrap-" + name)
        saved.mkdir(parents=True, exist_ok=True)
        native._verify_inputs(root, original_inputs)
        native._verify_inputs(workspace, initial)
        if tools is not None:
            current = vendor._base_state(
                infos[STAGES[0]], environment, tools, roots, project
            )
            for key in ("tools", "sdk", "environment"):
                if current[key] != profiles[STAGES[0]][key]:
                    raise ValueError("bootstrap immutable profile drift")
        window = _compile_engine_window(
            workspace,
            saved,
            environment,
            clients if tools is None else tools,
            engine_profile,
            name,
            kind,
        )
        completed, after = window.compile_result, window.after
        assess_compile_result(
            STAGES[1], completed.returncode, completed.stdout, project
        )
        info = _positive_info(workspace, saved, completed, kind)
        vendor._check_staged_sources(
            workspace, initial, project, producer_sources(workspace)[EXAMPLE]
        )
        selected_roots = _selection_sdk_roots(info, kind)
        observed_tools = vendor._tool_identities(info, workspace, environment, saved)
        for key, identity in clients.items():
            if (
                observed_tools[key].path != identity.path
                or observed_tools[key].sha256 != identity.sha256
            ):
                raise ValueError("launcher/tool identity changed")
        if tools is not None and (tools != observed_tools or roots != selected_roots):
            raise ValueError("native/forced tool or SDK root changed")
        tools, roots = observed_tools, selected_roots
        cell = _capture_cell_configuration(info, name, kind)
        profile = _selection_state(info, environment, tools, roots, project, name, kind)
        cells[name], profiles[name], infos[name] = cell, profile, info
        _retain_cell(saved, cell, "bootstrap-planned-configuration")
        vendor._retain_artifacts(
            info,
            saved,
            project,
            vendor._PropertyKind.ESP32_C6_MODE
            if kind is _SelectionProfile.C6
            else vendor._PropertyKind.ESP32_XTENSA_MODE,
        )
        vendor._json(
            saved / "bootstrap-scope.json",
            {
                "bootstrap_only": True,
                "certified_stage": False,
                "complete_executed_argv": False,
            },
        )
        vendor._json(saved / "frozen-profile.json", profile)
        engine_profile = after
    for name in STAGES[1:4]:
        _compare_cell_configurations(cells[STAGES[0]], cells[name])
        _compatible_cell_profiles(profiles[STAGES[0]], profiles[name])
    records = []
    for name in STAGES:
        cell_name = STAGES[1] if name == STAGES[4] else name
        cell, profile, info = cells[cell_name], profiles[cell_name], infos[cell_name]
        saved = artifacts / name
        saved.mkdir(parents=True, exist_ok=True)
        _activate_cell(cell)
        content = source_for_stage(original_source, name)
        (workspace / selection.SOURCE).write_bytes(content)
        expected = {**initial, selection.SOURCE: hashlib.sha256(content).hexdigest()}
        native._verify_inputs(root, original_inputs)
        native._verify_inputs(workspace, expected)
        release = Path(info["prog_path"]).parent
        # Keep prior build outputs visibly unconsumed. The known prior planned
        # database remains a frozen input to the failed compilation, never fresh.
        for filename in ("firmware.elf", "firmware.bin", "firmware.map"):
            prior = release / filename
            if prior.is_file():
                prior.rename(saved / ("unconsumed-prior-" + filename))
        session = _selection_build_session(
            workspace,
            saved,
            environment,
            tools,
            roots,
            profile,
            engine_profile,
            info,
            expected,
            name,
            kind,
        )
        outcome = assess_compile_result(
            name,
            session.compile_result.returncode,
            session.compile_result.stdout,
            project,
        )
        vendor._json(
            saved / "actual-build-result.json",
            {
                "returncode": outcome.returncode,
                "expected_compile_failure": outcome.expected_compile_failure,
                "complete_executed_argv": False,
            },
        )
        hashes = {}
        if not outcome.expected_compile_failure:
            updated = _positive_info(workspace, saved, session.compile_result, kind)
            observed = _selection_state(
                updated, environment, tools, roots, project, name, kind
            )
            vendor.property_check._verify_profile_state(
                saved, "actual-build-info", profile, observed
            )
            hashes = vendor._retain_artifacts(
                updated,
                saved,
                project,
                vendor._PropertyKind.ESP32_C6_MODE
                if kind is _SelectionProfile.C6
                else vendor._PropertyKind.ESP32_XTENSA_MODE,
            )
        config = _capture_cell_configuration(info, name, kind)
        configuration = _retain_cell(
            saved,
            config,
            "prior-input-planned-configuration"
            if outcome.expected_compile_failure
            else "planned-configuration",
        )
        for relative in _REQUIRED_SOURCES:
            path = saved / "staged-sources" / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            staged = (
                project / "lib/FastLED" / relative.removeprefix("src/")
                if relative.startswith("src/")
                else project / "src/sketch" / Path(relative).name
            )
            if (
                staged.is_symlink()
                or not staged.is_file()
                or vendor._digest(staged) != expected[relative]
            ):
                raise ValueError("controlled staged header/producer mismatch")
            path.write_bytes(staged.read_bytes())
            hashes[str(path)] = vendor._digest(path)
        native._verify_inputs(workspace, expected)
        native._verify_inputs(root, original_inputs)
        after_static = _selection_state(
            info, environment, tools, roots, project, name, kind
        )
        vendor.property_check._verify_profile_state(
            saved, "after-retention", profile, after_static
        )
        log = saved / "build.log"
        staged_bindings = _observe_staged_inputs(workspace, expected, project)
        stage = SelectionStage(
            name,
            project,
            outcome,
            log,
            vendor._digest(log),
            dict(expected),
            dict(expected),
            staged_bindings.expected,
            staged_bindings.observed,
            profile["tools"],
            after_static["tools"],
            profile,
            after_static,
            session.before,
            session.after,
            hashes,
            configuration,
        )
        validate_stage_record(stage)
        vendor._json(saved / "stage.json", asdict(stage))
        records.append(stage)
    return records


def _run_selection_proof(root: Path, kind: _SelectionProfile) -> SelectionProof:
    spec = _profile_spec(kind)
    root = root.resolve()
    for relative in (selection.SOURCE, EXAMPLE, HOOK, "ci/boards.py", "compile"):
        path = root / relative
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"missing or unsafe selection source/input {relative}")
    provider = _verify_provider_cohort()
    original_inputs = native._inputs(root)
    workspace, _ = native._snapshot(root, spec.snapshot_kind)
    artifacts = root / ".build" / spec.artifact_group / workspace.name
    originals = {
        relative: (workspace / relative).read_bytes() for relative in _REQUIRED_SOURCES
    }
    ini = workspace / ".build/fbuild" / spec.env / "platformio.ini"
    ini_existed = ini.exists()
    if ini.is_symlink() or ini_existed and not ini.is_file():
        raise ValueError("unsafe original project flag input")
    if ini_existed:
        originals[ini.relative_to(workspace).as_posix()] = ini.read_bytes()
    stages = None
    try:
        # Unit orchestration fixtures explicitly stub the preflight with no SDK
        # context; the real closed guard always returns the pinned typed cohort.
        if provider is not None:
            vendor._json(artifacts / "provider-cohort.json", asdict(provider))
        initial = dict(original_inputs)
        for relative, content in producer_sources(workspace).items():
            (workspace / relative).write_bytes(content)
            initial[relative] = hashlib.sha256(content).hexdigest()
        native._verify_inputs(workspace, initial)
        stages = _execute_fixed_stages(
            root,
            workspace,
            artifacts,
            initial,
            original_inputs,
            originals[selection.SOURCE],
            kind,
        )
        validate_attempt(stages, originals[selection.SOURCE])
    except (
        OSError,
        ValueError,
        KeyError,
        TypeError,
        native.CalledProcessError,
        native.TimeoutExpired,
    ) as error:
        vendor._json(
            artifacts / "failure.json",
            {"complete": False, "workspace": workspace, "error": str(error)},
        )
        raise ValueError(
            f"selection attempt failed; retained {workspace}: {error}"
        ) from error
    finally:
        pending = sys.exc_info()[1]
        errors = []
        if isinstance(pending, KeyboardInterrupt):
            try:
                vendor._json(
                    artifacts / "failure.json",
                    {
                        "complete": False,
                        "workspace": workspace,
                        "error": str(pending),
                        "type": type(pending).__name__,
                    },
                )
            except (OSError, ValueError, TypeError) as error:
                errors.append("interrupt diagnostic write failed: " + str(error))
        errors.extend(native._restore_sources(workspace, originals))
        try:
            if not ini_existed and ini.exists():
                artifacts.mkdir(parents=True, exist_ok=True)
                ini.rename(artifacts / "created-project-platformio.ini")
            native._verify_inputs(workspace, original_inputs)
            native._verify_inputs(root, original_inputs)
            if pending is None:
                _verify_final_dependencies(workspace, artifacts, stages, kind)
        except (OSError, ValueError, KeyError, TypeError) as error:
            errors.append(str(error))
        if errors:
            failure = {
                "complete": False,
                "workspace": workspace,
                "original_error": str(pending) if pending else None,
                "restoration_errors": errors,
            }
            if isinstance(pending, KeyboardInterrupt):
                try:
                    vendor._json(artifacts / "failure.json", failure)
                finally:
                    raise pending
            vendor._json(artifacts / "failure.json", failure)
            raise ValueError(
                f"selection restoration failed; retained {workspace}: {errors}"
            ) from pending
    proof = SelectionProof(workspace, stages, True, scope=spec.scope)
    vendor._json(artifacts / "proof.json", asdict(proof))
    return proof


@dataclass(frozen=True, slots=True)
class _StagedInputs:
    expected: dict[str, str]
    observed: dict[str, str]


def _staged_subset(inputs: dict[str, str]) -> dict[str, str]:
    return {
        relative: sha
        for relative, sha in inputs.items()
        if relative.startswith("src/") or relative.startswith("examples/AutoResearch/")
    }


def _observe_staged_inputs(
    workspace: Path, inputs: dict[str, str], project: Path
) -> _StagedInputs:
    # The complete generated project/library universes are checked independently;
    # these receipt maps name only the source/example inputs physically copied.
    vendor._check_staged_sources(
        workspace,
        inputs,
        project,
        producer_sources(Path(__file__).resolve().parents[1])[EXAMPLE],
    )
    expected = _staged_subset(inputs)
    observed = {}
    for relative, sha in expected.items():
        if relative.startswith("src/"):
            path = project / "lib/FastLED" / relative.removeprefix("src/")
        else:
            path = (
                project / "src/sketch" / relative.removeprefix("examples/AutoResearch/")
            )
        if path.is_symlink() or not path.is_file():
            raise ValueError("missing or unsafe physically staged input")
        observed[relative] = vendor._digest(path)
        if observed[relative] != sha:
            raise ValueError("physically staged source/example bytes changed")
    return _StagedInputs(expected, observed)


def _verify_final_dependencies(
    workspace: Path,
    artifacts: Path,
    stages: list[SelectionStage],
    kind: _SelectionProfile = _SelectionProfile.C6,
) -> None:
    if not stages or stages[-1].name != STAGES[-1]:
        raise ValueError("missing final dependency context")
    last = stages[-1]
    profile = last.profile_before
    required = ("tools", "sdk", "environment", "configuration", "planned_configuration")
    if any(
        not isinstance(profile.get(key), dict) or not profile[key] for key in required
    ):
        raise ValueError("missing frozen final base profile")
    info_file = artifacts / "restored/build-info.json"
    if str(info_file) not in last.artifact_sha256s:
        raise ValueError("final build-info is not an owned retained artifact")
    _verify_files(last.artifact_sha256s)
    _verify_files(last.configuration_sha256s)
    info = json.loads(info_file.read_text())[_profile_spec(kind).env]
    _validate_info_profile(info, kind)
    tools = {}
    try:
        for name, identity in profile["tools"].items():
            tools[name] = native.ToolIdentity(
                Path(identity["path"]), identity["sha256"], identity["version"]
            )
        project = workspace / ".build/fbuild" / _profile_spec(kind).env
        if Path(info["prog_path"]).parents[3] != project:
            raise ValueError("final build context belongs to another project")
        observed = vendor._base_state(
            info,
            _selection_environment(workspace),
            tools,
            _selection_sdk_roots(info, kind),
            project,
        )
    except (OSError, KeyError, TypeError, IndexError) as error:
        raise ValueError("unbound final dependency context") from error
    for key in ("tools", "sdk", "environment", "configuration"):
        if observed.get(key) != profile[key]:
            raise ValueError("final " + key + " dependency drift")
    # Restoring the entry's original INI is intentional and checked separately.
    # All other known recipe/rsp inputs remain byte-identical to the final cell.
    _verify_files(
        {
            path: sha
            for path, sha in profile["planned_configuration"].items()
            if Path(path) != project / "platformio.ini"
        }
    )
    vendor._json(
        artifacts / "final-dependencies.json",
        {
            "verified": True,
            "keys": ["tools", "sdk", "environment", "configuration"],
            "original_project_ini_restored_separately": True,
            "complete_executed_argv": False,
        },
    )


def run_c6_selection_proof(root: Path) -> SelectionProof:
    return _run_selection_proof(root, _SelectionProfile.C6)


def run_xtensa_selection_proof(root: Path) -> SelectionProof:
    return _run_selection_proof(root, _SelectionProfile.XTENSA)
