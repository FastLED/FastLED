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
import time
from contextvars import ContextVar
from dataclasses import asdict, dataclass, field
from enum import Enum
from pathlib import Path

from running_process import CompletedProcess

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


class _SelectionCell(Enum):
    FORCE1_NOMAP_ABSENT = "force1-nomap-absent"
    FORCE0_NOMAP0 = "force0-nomap0"
    FORCE0_NOMAP_ABSENT = "force0-nomap-absent"
    FORCE0_NOMAP1 = "force0-nomap1"
    FORCE1_NOMAP0 = "force1-nomap0"
    FORCE1_NOMAP1 = "force1-nomap1"


class _SelectionContext(Enum):
    SINGLE = "single"
    MATRIX_C6 = "matrix-c6"
    MATRIX_XTENSA = "matrix-xtensa"


_SELECTION_CONTEXT = ContextVar("selection_context", default=_SelectionContext.SINGLE)
_MATRIX_OUTER_SECONDS = 8 * 60 * 60


@dataclass(frozen=True, slots=True)
class _CellSpec:
    force_value: int
    no_pinmap_value: int | None


def _cell_spec(cell: _SelectionCell) -> _CellSpec:
    if cell is _SelectionCell.FORCE1_NOMAP_ABSENT:
        return _CellSpec(1, None)
    if cell is _SelectionCell.FORCE0_NOMAP0:
        return _CellSpec(0, 0)
    if cell is _SelectionCell.FORCE0_NOMAP_ABSENT:
        return _CellSpec(0, None)
    if cell is _SelectionCell.FORCE0_NOMAP1:
        return _CellSpec(0, 1)
    if cell is _SelectionCell.FORCE1_NOMAP0:
        return _CellSpec(1, 0)
    if cell is _SelectionCell.FORCE1_NOMAP1:
        return _CellSpec(1, 1)
    raise ValueError("unknown closed selection cell")


def _validate_profile_cell(kind: _SelectionProfile, cell: _SelectionCell) -> None:
    _profile_spec(kind)
    _cell_spec(cell)
    context = _SELECTION_CONTEXT.get()
    if context is _matrix_context(kind):
        return
    allowed = {_SelectionCell.FORCE1_NOMAP_ABSENT}
    if kind is _SelectionProfile.C6:
        allowed.add(_SelectionCell.FORCE0_NOMAP0)
    if cell not in allowed:
        raise ValueError("unsupported single-profile selection cell")


def _scope_for_profile_cell(kind: _SelectionProfile, cell: _SelectionCell) -> str:
    _validate_profile_cell(kind, cell)
    if cell is _SelectionCell.FORCE0_NOMAP0:
        return (
            "esp32c6-force0-NoMap0-native-inverse-idle900-compiler-selection-property"
        )
    return _profile_spec(kind).scope


def _artifact_group_for_cell(kind: _SelectionProfile, cell: _SelectionCell) -> str:
    _validate_profile_cell(kind, cell)
    if cell is _SelectionCell.FORCE0_NOMAP0:
        return "spi-esp32-c6-defined-zero-selection-proof"
    return _profile_spec(kind).artifact_group


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
        package_root=Path(str(distribution.locate_file("fbuild"))).absolute(),
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
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT


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
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT


@dataclass(frozen=True)
class SelectionBuildSession:
    compile_result: CompletedProcess
    before: dict
    after: dict
    static_after: dict


def stage_defines(
    name: str, cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT
) -> list[str]:
    spec = _cell_spec(cell)
    if name not in STAGES:
        raise ValueError("unknown fixed selection stage")
    scope = 1 if name == STAGES[2] else 2 if name == STAGES[3] else 0
    flags = [
        f"-DEXPECT_FORCED={int(name != STAGES[0])}",
        f"-DEXPECT_NO_PINMAP={int(spec.no_pinmap_value is not None)}",
    ]
    if name != STAGES[0]:
        flags.append(f"-DFASTLED_FORCE_SOFTWARE_PINS={spec.force_value}")
    if spec.no_pinmap_value is not None:
        flags.append(f"-DFASTLED_NO_PINMAP={spec.no_pinmap_value}")
    return [*flags, f"-DFASTLED_SELECTION_ASSERT_SCOPE={scope}"]


def validate_stage_defines(
    name: str,
    tokens: list[str],
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT,
) -> None:
    spec = _cell_spec(cell)
    if name not in STAGES:
        raise ValueError("unknown fixed selection stage")
    selection.validate_global_defines(
        tokens, None if name == STAGES[0] else spec.force_value, spec.no_pinmap_value
    )
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
    name: str,
    returncode: int,
    output: str,
    project: Path,
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT,
) -> SelectionOutcome:
    _cell_spec(cell)
    if name not in STAGES or type(returncode) is not int or not isinstance(output, str):
        raise ValueError("unknown fixed selection result")
    negative = name in STAGES[2:4]
    if not negative:
        selection.validate_stage_output("baseline", returncode, output)
    else:
        if returncode != 1:
            raise ValueError("scoped mutant must be an actual rejected compile")
        expected = _MATRIX_DIAGNOSTICS.get() or _scoped_diagnostics(name)
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
    return SelectionOutcome(returncode, negative, not negative, not negative, cell=cell)


def _verify_files(hashes: dict[str, str]) -> None:
    for name, sha in hashes.items():
        path = Path(name)
        if not path.is_file() or path.is_symlink() or vendor._digest(path) != sha:
            raise ValueError("retained selection artifact/configuration changed")


def validate_stage_record(
    stage: SelectionStage,
    expected_cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT,
) -> None:
    _cell_spec(expected_cell)
    if not isinstance(stage, SelectionStage) or stage.name not in STAGES:
        raise ValueError("unbound typed selection stage")
    if stage.cell is not expected_cell or stage.outcome.cell is not expected_cell:
        raise ValueError("selection stage/outcome belongs to another cell")
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
    owner = _MATRIX_OWNER.get()
    expected = (
        _assess_matrix_stage(
            owner.profile,
            expected_cell,
            stage.name,
            stage.outcome.returncode,
            stage.log_path.read_text(),
            stage.project,
        )
        if owner is not None
        else assess_compile_result(
            stage.name,
            stage.outcome.returncode,
            stage.log_path.read_text(),
            stage.project,
            expected_cell,
        )
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
    _validate_retained_cell_bindings(stage, expected_cell)


def _selection_environment(workspace: Path) -> dict[str, str]:
    # Explicit nondefault condition for this proof only. fb63 context.rs reads
    # this process variable in the live maintenance loop (entry/mod.rs528/531).
    # Health/status traffic is not work and does not renew an idle lease.
    environment = vendor._environment(workspace)
    environment["FBUILD_SELF_EVICTION_SECS"] = "900"
    return environment


def _run_fixed_compile(
    workspace: Path,
    saved: Path,
    environment: dict,
    name: str,
    kind: _SelectionProfile,
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT,
):
    _validate_profile_cell(kind, cell)
    spec = _profile_spec(kind)
    saved.mkdir(parents=True, exist_ok=True)
    command = [
        "bash",
        "compile",
        spec.env,
        "--examples",
        "AutoResearch",
        "--defines",
        ",".join(flag.removeprefix("-D") for flag in stage_defines(name, cell)),
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


def _run_c6_compile(
    workspace: Path,
    saved: Path,
    environment: dict,
    name: str,
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT,
):
    return _run_fixed_compile(
        workspace, saved, environment, name, _SelectionProfile.C6, cell
    )


def _run_xtensa_compile(
    workspace: Path,
    saved: Path,
    environment: dict,
    name: str,
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT,
):
    return _run_fixed_compile(
        workspace, saved, environment, name, _SelectionProfile.XTENSA, cell
    )


@dataclass(frozen=True, slots=True)
class _CompileWindow:
    compile_result: CompletedProcess
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
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT,
    *,
    calibration: "_MatrixCalibrationSpec | None" = None,
) -> _CompileWindow:
    _validate_profile_cell(kind, cell)
    if calibration is not None:
        _require_calibration(kind, calibration)
        if calibration.cell is not cell or calibration.name != name:
            raise ValueError("calibration window tuple/stage mismatch")
    _check_matrix_deadline(reserve_build=True)
    vendor._ensure_stage_engine(workspace, saved, environment, tools, engine_profile)
    before = vendor._daemon_identity(
        workspace, saved, "build-before", environment, tools
    )
    vendor._verify_engine_profile(saved, "build-before", engine_profile, before)
    after = None
    try:
        completed = (
            _run_matrix_calibration_compile(
                workspace, saved, environment, kind, calibration
            )
            if calibration is not None
            else _run_matrix_compile(workspace, saved, environment, name, kind, cell)
            if _SELECTION_CONTEXT.get() is _matrix_context(kind)
            else _run_c6_compile(workspace, saved, environment, name, cell)
            if kind is _SelectionProfile.C6
            else _run_xtensa_compile(workspace, saved, environment, name, cell)
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
    _check_matrix_deadline()
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
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT,
) -> SelectionBuildSession:
    _validate_profile_cell(kind, cell)
    project = workspace / ".build/fbuild" / _profile_spec(kind).env
    before_static = _selection_state(
        info, environment, tools, roots, project, name, kind, cell
    )
    vendor.property_check._verify_profile_state(saved, "before", profile, before_static)
    window = _compile_engine_window(
        workspace, saved, environment, tools, engine_profile, name, kind, cell
    )
    after_static = _selection_state(
        info, environment, tools, roots, project, name, kind, cell
    )
    vendor.property_check._verify_profile_state(
        saved, "after-build", profile, after_static
    )
    native._verify_inputs(workspace, expected_inputs)
    vendor._check_staged_sources(
        workspace,
        expected_inputs,
        project,
        _bound_producer(workspace, expected_inputs),
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
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT


def _normalize_cell_tokens(
    tokens: list[str],
    name: str,
    require_defines: bool = True,
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT,
) -> list[str]:
    force = None if name == STAGES[0] else _cell_spec(cell).force_value
    if require_defines:
        validate_stage_defines(name, tokens, cell)
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
            for flag in stage_defines(name, cell)
            if flag[2:].partition("=")[0] not in present
        ]
        validate_stage_defines(name, [*tokens, *missing], cell)
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
                    else str(_cell_spec(cell).force_value)
                )
                if expected is None or (value if separator else "1") != expected:
                    raise ValueError("unexpected cell macro operand")
                continue
        result.extend(consumed)
    return result


def _capture_cell_configuration(
    info: dict,
    name: str,
    kind: _SelectionProfile = _SelectionProfile.C6,
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT,
) -> CellConfiguration:
    _validate_profile_cell(kind, cell)
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
    normalized_ini: dict[str, dict[str, str | list[str]]] = {
        section: dict(parser.items(section, raw=True)) for section in parser.sections()
    }
    flags = shlex.split(parser.get(section, "build_flags", raw=True), comments=True)
    normalized_ini[section]["build_flags"] = _normalize_cell_tokens(
        flags, name, cell=cell
    )
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
                        "arguments": _normalize_cell_tokens(
                            arguments, name, False, cell
                        ),
                        "effective_arguments": _normalize_cell_tokens(
                            effective, name, cell=cell
                        ),
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
                shlex.split(path.read_text()), name, False, cell
            )
    contents = {str(path): path.read_bytes() for path in sorted(files)}
    return CellConfiguration(
        name,
        {str(path): vendor._digest(path) for path in sorted(files)},
        contents,
        {"ini": normalized_ini, "recipes": normalized, "prog_path": info["prog_path"]},
        cell,
    )


def _compare_cell_configurations(
    native_cell: CellConfiguration, forced_cell: CellConfiguration
) -> None:
    if native_cell.cell is not forced_cell.cell:
        raise ValueError("cannot combine different selection cells")
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
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT,
) -> dict:
    _validate_profile_cell(kind, cell)
    result = vendor._base_state(info, environment, tools, roots, project)
    result["planned_configuration"] = _capture_cell_configuration(
        info, name, kind, cell
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
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT


def validate_attempt(
    stages: list[SelectionStage],
    original_source: bytes,
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT,
) -> None:
    validate_stage_sequence(tuple(stage.name for stage in stages))
    owner = _MATRIX_OWNER.get()
    controlled = (
        {selection.SOURCE, EXAMPLE, HOOK} if owner is not None else {selection.SOURCE}
    )
    first = stages[0]
    unchanged = {
        key: value
        for key, value in first.inputs_before.items()
        if key not in controlled
    }
    forced_profile = stages[1].profile_before
    for stage in stages:
        validate_stage_record(stage, cell)
        if owner is not None:
            for inputs in (
                stage.inputs_before,
                stage.inputs_after,
                stage.staged_sources_expected,
                stage.staged_sources_observed,
            ):
                _validate_matrix_payloads(
                    _matrix_code_root(), owner.profile, cell, stage.name, inputs
                )
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
            if key not in controlled
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


def _validate_retained_cell_bindings(
    stage: SelectionStage, cell: _SelectionCell
) -> None:
    """Replay effective macros from hash-bound retained configuration bytes."""
    paths = {Path(name): Path(name) for name in stage.configuration_sha256s}
    for path in tuple(paths):
        if path.name.endswith("-scope.json"):
            descriptor = json.loads(path.read_text())
            for source, retained in descriptor["retained_files"].items():
                target = Path(retained)
                if str(target) not in stage.configuration_sha256s:
                    raise ValueError("unbound retained configuration")
                paths[Path(source)] = target
    ini_paths = {
        target
        for source, target in paths.items()
        if source.name.endswith("platformio.ini")
    }
    databases = {
        target
        for source, target in paths.items()
        if source.name.endswith("compile_commands.raw.json")
    }
    if len(ini_paths) != 1 or not databases:
        raise ValueError("missing retained cell configuration")
    parser = configparser.ConfigParser(
        interpolation=None, inline_comment_prefixes=("#", ";"), strict=True
    )
    parser.read_string(next(iter(ini_paths)).read_text())
    sections = [section for section in parser.sections() if section.startswith("env:")]
    if parser.defaults() or len(sections) != 1:
        raise ValueError("ambiguous retained cell configuration")
    validate_stage_defines(
        stage.name,
        shlex.split(parser.get(sections[0], "build_flags", raw=True), comments=True),
        cell,
    )

    def expand(
        tokens: list[str], directory: Path, active: frozenset[Path] = frozenset()
    ) -> list[str]:
        result = []
        for token in tokens:
            if not token.startswith("@"):
                result.append(token)
                continue
            source = (directory / token[1:]).resolve()
            if source in active or source not in paths:
                raise ValueError("unbound retained response file")
            result.extend(
                expand(
                    shlex.split(paths[source].read_text()), directory, active | {source}
                )
            )
        return result

    for database in databases:
        rows = json.loads(database.read_text())
        if not isinstance(rows, list) or not rows:
            raise ValueError("missing retained planned rows")
        for row in rows:
            tokens = row.get("arguments")
            if tokens is None:
                tokens = shlex.split(row["command"])
            validate_stage_defines(
                stage.name, expand(tokens, Path(row["directory"])), cell
            )


def _retain_cell(saved: Path, cell: CellConfiguration, label: str) -> dict[str, str]:
    result = {}
    retained_files = {}
    for index, (source, content) in enumerate(sorted(cell.contents.items())):
        path = saved / label / f"{index:04d}-{Path(source).name}"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        result[str(path)] = vendor._digest(path)
        retained_files[source] = str(path)
    descriptor = saved / (label + "-scope.json")
    vendor._json(
        descriptor,
        {
            "planned_only": True,
            "freshness": "unproven",
            "source_sha256s": cell.hashes,
            "retained_files": retained_files,
            "complete_executed_argv": False,
        },
    )
    result[str(descriptor)] = vendor._digest(descriptor)
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
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT,
) -> list[SelectionStage]:
    _validate_profile_cell(kind, cell)
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
        stage_initial = _install_matrix_sources(workspace, kind, cell, name, initial)
        native._verify_inputs(root, original_inputs)
        native._verify_inputs(workspace, stage_initial)
        if tools is not None and roots is not None:
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
            cell,
        )
        completed, after = window.compile_result, window.after
        assess_compile_result(
            STAGES[1], completed.returncode, completed.stdout, project, cell
        )
        info = _positive_info(workspace, saved, completed, kind)
        vendor._check_staged_sources(
            workspace, stage_initial, project, _bound_producer(workspace, stage_initial)
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
        cfg_cell = _capture_cell_configuration(info, name, kind, cell)
        profile = _selection_state(
            info, environment, tools, roots, project, name, kind, cell
        )
        cells[name], profiles[name], infos[name] = cfg_cell, profile, info
        _retain_cell(saved, cfg_cell, "bootstrap-planned-configuration")
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
    if tools is None or roots is None:
        raise ValueError("bootstrap did not observe tools and SDK roots")
    for name in STAGES[1:4]:
        _compare_cell_configurations(cells[STAGES[0]], cells[name])
        _compatible_cell_profiles(profiles[STAGES[0]], profiles[name])
    records = []
    for name in STAGES:
        cell_name = STAGES[1] if name == STAGES[4] else name
        cfg_cell, profile, info = (
            cells[cell_name],
            profiles[cell_name],
            infos[cell_name],
        )
        saved = artifacts / name
        saved.mkdir(parents=True, exist_ok=True)
        _activate_cell(cfg_cell)
        stage_initial = _install_matrix_sources(workspace, kind, cell, name, initial)
        content = source_for_stage(original_source, name)
        (workspace / selection.SOURCE).write_bytes(content)
        expected = {
            **stage_initial,
            selection.SOURCE: hashlib.sha256(content).hexdigest(),
        }
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
            cell,
        )
        outcome = (
            _assess_matrix_stage(
                kind,
                cell,
                name,
                session.compile_result.returncode,
                session.compile_result.stdout,
                project,
            )
            if _SELECTION_CONTEXT.get() is _matrix_context(kind)
            else assess_compile_result(
                name,
                session.compile_result.returncode,
                session.compile_result.stdout,
                project,
                cell,
            )
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
                updated, environment, tools, roots, project, name, kind, cell
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
        config = _capture_cell_configuration(info, name, kind, cell)
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
            info, environment, tools, roots, project, name, kind, cell
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
            cell,
        )
        validate_stage_record(stage, cell)
        vendor._json(saved / "stage.json", asdict(stage))
        records.append(stage)
    if _SELECTION_CONTEXT.get() is _matrix_context(kind):
        _MATRIX_RUNTIME.set(
            _MatrixRuntime(
                info,
                environment,
                tools,
                roots,
                profiles[STAGES[1]],
                records[-1].serving_engine_after,
                config.contents,
            )
        )
    return records


def _run_selection_proof(
    root: Path,
    kind: _SelectionProfile,
    cell: _SelectionCell = _SelectionCell.FORCE1_NOMAP_ABSENT,
) -> SelectionProof:
    _validate_profile_cell(kind, cell)
    spec = _profile_spec(kind)
    root = root.resolve()
    for relative in (selection.SOURCE, EXAMPLE, HOOK, "ci/boards.py", "compile"):
        path = root / relative
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"missing or unsafe selection source/input {relative}")
    provider = _verify_provider_cohort()
    original_inputs = native._inputs(root)
    workspace, _ = native._snapshot(root, spec.snapshot_kind)
    artifacts = root / ".build" / _artifact_group_for_cell(kind, cell) / workspace.name
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
            cell,
        )
        validate_attempt(stages, originals[selection.SOURCE], cell)
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
    proof = SelectionProof(
        workspace, stages, True, scope=_scope_for_profile_cell(kind, cell), cell=cell
    )
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
        _bound_producer(workspace, inputs),
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


def run_c6_defined_zero_selection_proof(root: Path) -> SelectionProof:
    return _run_selection_proof(
        root, _SelectionProfile.C6, _SelectionCell.FORCE0_NOMAP0
    )


@dataclass(frozen=True, slots=True)
class _MatrixSubmission:
    cell: _SelectionCell
    name: str
    certified: bool
    calibration: "_MatrixCalibrationSpec | None" = None


@dataclass(frozen=True, slots=True)
class SelectionMatrixCell:
    profile: _SelectionProfile
    cell: _SelectionCell
    stages: list[SelectionStage]
    discovery_submissions: int
    discovery_sha256s: dict[str, str] = field(default_factory=dict)


@dataclass(frozen=True, slots=True)
class SelectionMatrixProof:
    workspace: Path
    profile: _SelectionProfile
    cells: list[SelectionMatrixCell]
    restoration_verified: bool
    calibrations: list["SelectionMatrixCalibration"] = field(default_factory=list)
    calibration_submissions: int = 16
    numeric_values_verified: bool = False
    oracle_values_verified: bool = False
    discovery_submissions: int = 24
    certified_submissions: int = 30
    idle_policy_seconds: int = 900
    outer_deadline_seconds: int = _MATRIX_OUTER_SECONDS
    scope: str = "complete-literal-compiler-selection-matrix-only"
    runtime_gpio_proven: bool = False
    complete_executed_argv: bool = False
    full_finding_qualified: bool = False
    limitation: str = "Nine literal configurations, compiler ancestry/HAS only. Independent native inverse and paired guard-mutant submissions for every forced cell. Sixteen additional paired literal/oracle counter-controls. No GPIO execution, mask qualification, MY support, firmware determinism, timing/cache or full finding qualification. Idle900 is not a lease; process identity is checked within each build, immutable engine semantics across builds."


def _matrix_context(kind: _SelectionProfile) -> _SelectionContext:
    _profile_spec(kind)
    return (
        _SelectionContext.MATRIX_C6
        if kind is _SelectionProfile.C6
        else _SelectionContext.MATRIX_XTENSA
    )


def _matrix_cells(kind: _SelectionProfile) -> tuple[_SelectionCell, ...]:
    _profile_spec(kind)
    return (
        _SelectionCell.FORCE0_NOMAP_ABSENT,
        _SelectionCell.FORCE0_NOMAP0,
        _SelectionCell.FORCE0_NOMAP1,
        _SelectionCell.FORCE1_NOMAP_ABSENT,
        _SelectionCell.FORCE1_NOMAP0,
        _SelectionCell.FORCE1_NOMAP1,
    )


def _validate_matrix_sequence(
    kind: _SelectionProfile, cells: tuple[_SelectionCell, ...]
) -> None:
    if tuple(cells) != _matrix_cells(kind):
        raise ValueError("incomplete, reordered or foreign matrix cell sequence")


def _matrix_submission_plan(kind: _SelectionProfile) -> tuple[_MatrixSubmission, ...]:
    return tuple(
        _MatrixSubmission(cell, name, certified)
        for cell in _matrix_cells(kind)
        for certified, names in ((False, STAGES[:4]), (True, STAGES))
        for name in names
    )


def _run_matrix_compile(
    workspace: Path,
    saved: Path,
    environment: dict,
    name: str,
    kind: _SelectionProfile,
    cell: _SelectionCell,
):
    if cell not in _matrix_cells(kind) or name not in STAGES:
        raise ValueError("unknown matrix compile profile/cell/stage")
    token = _SELECTION_CONTEXT.set(_matrix_context(kind))
    try:
        if kind is _SelectionProfile.C6:
            return _run_c6_compile(workspace, saved, environment, name, cell)
        return _run_xtensa_compile(workspace, saved, environment, name, cell)
    finally:
        _SELECTION_CONTEXT.reset(token)


def _matrix_configuration_sources(stage: SelectionStage) -> dict[Path, Path]:
    """Resolve only already hash-bound copies, never mutable generated files."""
    _verify_files(stage.configuration_sha256s)
    descriptors = [
        Path(name)
        for name in stage.configuration_sha256s
        if name.endswith("-scope.json")
    ]
    if not descriptors:
        # Synthetic schema carriers use direct files, not managed SDK records.
        return {Path(name): Path(name) for name in stage.configuration_sha256s}
    if len(descriptors) != 1:
        raise ValueError("ambiguous matrix retained configuration descriptor")
    descriptor = json.loads(descriptors[0].read_text())
    result = {}
    for source, retained in descriptor["retained_files"].items():
        if retained not in stage.configuration_sha256s:
            raise ValueError("unbound matrix recipe copy")
        result[Path(source)] = Path(retained)
    return result


def _matrix_strip_definitions(tokens: list[str]) -> list[str]:
    # Exact effective scalar semantics have already been checked by the stage
    # validator, including expanded response files. Remove only the five fixed
    # configuration operands; all compiler options and their ordering survive.
    keys = {
        "EXPECT_FORCED",
        "EXPECT_NO_PINMAP",
        "FASTLED_FORCE_SOFTWARE_PINS",
        "FASTLED_NO_PINMAP",
        "FASTLED_SELECTION_ASSERT_SCOPE",
    }
    result = []
    index = 0
    while index < len(tokens):
        token = tokens[index]
        index += 1
        operand = token[2:] if token.startswith("-D") else None
        original = [token]
        if token == "-D":
            if index >= len(tokens):
                raise ValueError("missing matrix macro operand")
            operand = tokens[index]
            original.append(operand)
            index += 1
        if operand is not None and operand.partition("=")[0] in keys:
            continue
        result.extend(original)
    return result


def _matrix_recipe_binding(stage: SelectionStage, kind: _SelectionProfile) -> dict:
    sources = _matrix_configuration_sources(stage)
    spec = _profile_spec(kind)
    if stage.project.name != spec.env:
        raise ValueError("retained matrix project environment/profile mismatch")
    result = {}
    described = any(
        name.endswith("-scope.json") for name in stage.configuration_sha256s
    )
    for source, retained in sources.items():
        # Real descriptors retain original fixed project paths. Synthetic files
        # have distinct schema directories and compare by their fixed basenames.
        key = str(source) if described else source.name
        if source.name.endswith("platformio.ini"):
            parser = configparser.ConfigParser(
                interpolation=None, inline_comment_prefixes=("#", ";"), strict=True
            )
            parser.read_string(retained.read_text())
            selected = "env:" + spec.env
            if (
                parser.defaults()
                or not parser.has_section(selected)
                or any(
                    name.startswith("env:") and name != selected
                    for name in parser.sections()
                )
            ):
                raise ValueError("retained matrix environment/profile binding mismatch")
            board = parser.get(selected, "board", fallback=None)
            expected_board = (
                "esp32-c6-devkitc-1" if kind is _SelectionProfile.C6 else "esp32dev"
            )
            if board is not None and board != expected_board:
                raise ValueError("retained matrix board/profile binding mismatch")
            values: dict[str, dict[str, str | list[str]]] = {
                name: dict(parser.items(name, raw=True)) for name in parser.sections()
            }
            flags = shlex.split(
                parser.get(selected, "build_flags", raw=True), comments=True
            )
            validate_stage_defines(stage.name, flags, stage.cell)
            values[selected]["build_flags"] = _matrix_strip_definitions(flags)
            result[key] = values
        elif source.name.endswith("compile_commands.raw.json"):
            rows = json.loads(retained.read_text())
            normalized = []
            for row in rows:
                item = dict(row)
                tokens = row.get("arguments")
                if tokens is None:
                    tokens = shlex.split(row["command"])
                item.pop("command", None)
                item["arguments"] = _matrix_strip_definitions(tokens)
                normalized.append(item)
            result[key] = normalized
        elif source.suffix == ".rsp":
            result[key] = _matrix_strip_definitions(shlex.split(retained.read_text()))
        else:
            raise ValueError("unreviewed matrix configuration file")
    return result


def _validate_matrix_results(
    kind: _SelectionProfile,
    results: list[SelectionMatrixCell],
    original_source: bytes,
    workspace: Path,
) -> None:
    _profile_spec(kind)
    if any(not isinstance(result, SelectionMatrixCell) for result in results):
        raise ValueError("untyped matrix cell record")
    _validate_matrix_sequence(kind, tuple(result.cell for result in results))
    if any(result.profile is not kind for result in results):
        raise ValueError("matrix architecture/profile mismatch")
    owner = _matrix_owner(kind, workspace)
    artifacts = _matrix_owned_artifacts(owner)
    for result in results:
        validate_stage_sequence(tuple(stage.name for stage in result.stages))
    first = results[0].stages[0]
    tools = first.tools_before
    engine = vendor._engine_semantics(first.serving_engine_before)
    profiles = {
        key: value
        for key, value in first.profile_before.items()
        if key not in {"planned_configuration", "cell"}
    }
    recipes = {}
    for result in results:
        if result.discovery_submissions != 4:
            raise ValueError("matrix needs four fresh discovery submissions per cell")
        _verify_files(result.discovery_sha256s)
        required_discovery = {
            artifacts / result.cell.value / ("bootstrap-" + name) / filename
            for name in STAGES[:4]
            for filename in ("build.log", "bootstrap-scope.json")
        }
        for filename in result.discovery_sha256s:
            path = Path(filename)
            directories = [
                artifacts / result.cell.value / ("bootstrap-" + name)
                for name in STAGES[:4]
            ]
            matching = [
                directory
                for directory in directories
                if path.absolute().is_relative_to(directory.absolute())
            ]
            if len(matching) != 1:
                raise ValueError("foreign matrix discovery cell/stage ownership")
            _require_matrix_owned_file(path, matching[0], owner)
        if not required_discovery <= {Path(path) for path in result.discovery_sha256s}:
            raise ValueError("missing owned matrix discovery stage records")
        validate_attempt(result.stages, original_source, result.cell)
        for stage in result.stages:
            if "cell" in stage.profile_before and stage.profile_before["cell"] != (
                "native" if stage.name == STAGES[0] else "forced"
            ):
                raise ValueError("unexpected synthetic profile cell category")
            # Authenticate architecture/project/INI before ownership errors, so
            # coherent outer-tag relabeling cannot be masked by a path check.
            recipe = _matrix_recipe_binding(stage, kind)
            expected_project = workspace / ".build/fbuild" / _profile_spec(kind).env
            if stage.project != expected_project:
                raise ValueError("foreign matrix project/environment profile binding")
            retained = {
                *stage.artifact_sha256s,
                *stage.configuration_sha256s,
                str(stage.log_path),
            }
            stage_directory = artifacts / result.cell.value / stage.name
            for filename in retained:
                _require_matrix_owned_file(Path(filename), stage_directory, owner)
            if stage.tools_before != tools:
                raise ValueError("cross-cell matrix tools drift")
            observed = {
                key: value
                for key, value in stage.profile_before.items()
                if key not in {"planned_configuration", "cell"}
            }
            if observed != profiles:
                raise ValueError(
                    "cross-cell matrix SDK/environment/configuration drift"
                )
            if vendor._engine_semantics(stage.serving_engine_before) != engine:
                raise ValueError("cross-cell matrix serving provider semantics drift")
            if stage.name in recipes and recipes[stage.name] != recipe:
                raise ValueError(
                    "cross-cell recipe changed beyond fixed macro definitions"
                )
            recipes[stage.name] = recipe


def _execute_matrix_cells(
    root: Path,
    workspace: Path,
    artifacts: Path,
    initial: dict,
    original_inputs: dict,
    original_source: bytes,
    kind: _SelectionProfile,
) -> list[SelectionMatrixCell]:
    cells = _matrix_cells(kind)
    context = _SELECTION_CONTEXT.set(_matrix_context(kind))
    deadline = _MATRIX_DEADLINE.set(
        _MATRIX_DEADLINE.get() or time.monotonic() + _MATRIX_OUTER_SECONDS
    )
    results = []
    try:
        for cell in cells:
            _check_matrix_deadline()
            saved = artifacts / cell.value
            stages = _execute_fixed_stages(
                root,
                workspace,
                saved,
                initial,
                original_inputs,
                original_source,
                kind,
                cell,
            )
            discovery = {}
            for name in STAGES[:4]:
                directory = saved / ("bootstrap-" + name)
                for filename in ("build.log", "bootstrap-scope.json"):
                    path = directory / filename
                    if path.is_symlink() or not path.is_file():
                        raise ValueError("missing actual matrix discovery record")
                    discovery[str(path)] = vendor._digest(path)
                for path in directory.rglob("*"):
                    if path.is_symlink():
                        raise ValueError("unsafe retained matrix discovery file")
                    if path.is_file():
                        discovery[str(path)] = vendor._digest(path)
            results.append(SelectionMatrixCell(kind, cell, stages, 4, discovery))
        _check_matrix_deadline()
    finally:
        _MATRIX_DEADLINE.reset(deadline)
        _SELECTION_CONTEXT.reset(context)
    return results


def _verify_matrix_final_dependencies(
    workspace: Path,
    artifacts: Path,
    results: list[SelectionMatrixCell],
    kind: _SelectionProfile,
    calibrations: list["SelectionMatrixCalibration"] | None = None,
) -> None:
    _validate_matrix_sequence(kind, tuple(result.cell for result in results))
    if (
        any(not result.discovery_sha256s for result in results)
        or not calibrations
        or len(calibrations) != 16
    ):
        raise ValueError(
            "missing actual matrix discovery/calibration dependency context"
        )
    runtime = _MATRIX_RUNTIME.get()
    if not isinstance(runtime, _MatrixRuntime) or not runtime.configuration:
        raise ValueError("missing final actual calibration runtime context")
    last = results[-1].stages[-1]
    info_file = artifacts / results[-1].cell.value / "restored/build-info.json"
    if str(info_file) not in last.artifact_sha256s:
        raise ValueError("final positive build-info is not an owned retained artifact")
    _verify_files(last.artifact_sha256s)
    info = json.loads(info_file.read_text())[_profile_spec(kind).env]
    required = ("tools", "sdk", "environment", "configuration")
    if any(
        not isinstance(runtime.profile.get(key), dict) or not runtime.profile[key]
        for key in required
    ):
        raise ValueError("missing frozen final SDK/tools/environment base profile")
    project = workspace / ".build/fbuild" / _profile_spec(kind).env
    observed = _matrix_calibration_state(
        info,
        _selection_environment(workspace),
        runtime.tools,
        _selection_sdk_roots(info, kind),
        project,
        kind,
        calibrations[-1].control,
    )
    for key in required:
        if observed.get(key) != runtime.profile[key]:
            raise ValueError("final matrix " + key + " dependency drift")
    # Entry INI restoration is intentional. All other post-failure recipe
    # files remain the observed bound inputs; none is called fresh executed argv.
    _verify_files(
        {
            name: hashlib.sha256(data).hexdigest()
            for name, data in runtime.configuration.items()
            if Path(name) != project / "platformio.ini"
        }
    )
    vendor._json(
        artifacts / "final-dependencies.json",
        {
            "verified": list(required),
            "entry_ini_restored_separately": True,
            "recipe_freshness": "unproven",
            "complete_executed_argv": False,
        },
    )


_MATRIX_DEADLINE: ContextVar[float | None] = ContextVar(
    "selection_matrix_deadline", default=None
)


def _check_matrix_deadline(*, reserve_build: bool = False) -> None:
    deadline = _MATRIX_DEADLINE.get()
    # native._run gives each actual build its reviewed 1800-second timeout.
    # Never start a build whose full window would exceed the outer budget.
    remaining = 1800 if reserve_build else 0
    if deadline is not None and time.monotonic() + remaining >= deadline:
        raise ValueError("eight-hour matrix deadline exceeded; no negative acceptance")


def _run_selection_matrix_owned(
    root: Path, kind: _SelectionProfile
) -> SelectionMatrixProof:
    _matrix_cells(kind)
    spec = _profile_spec(kind)
    root = root.resolve()
    for relative in (selection.SOURCE, EXAMPLE, HOOK, "ci/boards.py", "compile"):
        path = root / relative
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"missing or unsafe selection source/input {relative}")
    provider = _verify_provider_cohort()
    original_inputs = native._inputs(root)
    workspace, _ = native._snapshot(root, spec.snapshot_kind)
    _MATRIX_OWNER.set(_MatrixOwner(root, workspace, kind))
    artifacts = (
        root
        / ".build"
        / (_profile_spec(kind).artifact_group + "-matrix")
        / workspace.name
    )
    originals = {
        relative: (workspace / relative).read_bytes() for relative in _REQUIRED_SOURCES
    }
    ini = workspace / ".build/fbuild" / spec.env / "platformio.ini"
    ini_existed = ini.exists()
    if ini.is_symlink() or ini_existed and not ini.is_file():
        raise ValueError("unsafe original project flag input")
    if ini_existed:
        originals[ini.relative_to(workspace).as_posix()] = ini.read_bytes()
    results = None
    calibrations = None
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
        results = _execute_matrix_cells(
            root,
            workspace,
            artifacts,
            initial,
            original_inputs,
            originals[selection.SOURCE],
            kind,
        )
        _validate_matrix_results(kind, results, originals[selection.SOURCE], workspace)
        calibrations = _execute_matrix_calibrations(
            root,
            workspace,
            artifacts,
            initial,
            original_inputs,
            originals[selection.SOURCE],
            kind,
            results,
        )
        _validate_matrix_calibrations(
            kind, calibrations, results, originals[selection.SOURCE], workspace
        )
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
                _verify_matrix_final_dependencies(
                    workspace, artifacts, results, kind, calibrations
                )
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
    # Retained earlier files are re-read AFTER final physical dependency checks
    # and restoration, immediately before aggregate publication.
    _validate_matrix_results(kind, results, originals[selection.SOURCE], workspace)
    _validate_matrix_calibrations(
        kind, calibrations, results, originals[selection.SOURCE], workspace
    )
    proof = SelectionMatrixProof(
        workspace,
        kind,
        results,
        True,
        calibrations=calibrations,
        numeric_values_verified=True,
        oracle_values_verified=True,
    )
    vendor._json(artifacts / "proof.json", asdict(proof))
    return proof


def run_c6_selection_matrix_proof(root: Path) -> SelectionMatrixProof:
    return _run_selection_matrix(root, _SelectionProfile.C6)


def run_xtensa_selection_matrix_proof(root: Path) -> SelectionMatrixProof:
    return _run_selection_matrix(root, _SelectionProfile.XTENSA)


@dataclass(frozen=True, slots=True)
class _MatrixOwner:
    root: Path
    workspace: Path
    profile: _SelectionProfile


_MATRIX_OWNER: ContextVar[_MatrixOwner | None] = ContextVar(
    "selection_matrix_owner", default=None
)


def _matrix_owned_artifacts(owner: _MatrixOwner) -> Path:
    return (
        owner.root.resolve()
        / ".build"
        / (_profile_spec(owner.profile).artifact_group + "-matrix")
        / owner.workspace.name
    )


def _matrix_owner(kind: _SelectionProfile, workspace: Path) -> _MatrixOwner:
    owner = _MATRIX_OWNER.get()
    if not isinstance(owner, _MatrixOwner) or owner.profile is not kind:
        raise ValueError("missing or foreign matrix architecture/profile ownership")
    if owner.workspace.resolve() != workspace.resolve():
        raise ValueError("foreign matrix attempt workspace ownership")
    return owner


def _require_matrix_owned_file(
    path: Path, directory: Path, owner: _MatrixOwner
) -> None:
    # Retained copies have no symlink ancestry or lexical traversal. Comparing
    # to the exact derived stage directory prevents borrowing a sibling cell,
    # another attempt, or a different stage under the same broad .build root.
    absolute = path.absolute()
    if absolute != path.resolve() or not absolute.is_relative_to(directory.absolute()):
        raise ValueError(
            "foreign retained matrix attempt/cell/stage artifact ownership"
        )
    for parent in (absolute, *absolute.parents):
        if parent.is_symlink():
            raise ValueError("unsafe matrix retained artifact ancestry")
        if parent == owner.root.resolve():
            break


def _run_selection_matrix(root: Path, kind: _SelectionProfile) -> SelectionMatrixProof:
    _matrix_cells(kind)
    token = _MATRIX_OWNER.set(None)
    deadline = _MATRIX_DEADLINE.set(time.monotonic() + _MATRIX_OUTER_SECONDS)
    runtime = _MATRIX_RUNTIME.set(None)
    try:
        return _run_selection_matrix_owned(root, kind)
    finally:
        _MATRIX_RUNTIME.reset(runtime)
        _MATRIX_DEADLINE.reset(deadline)
        _MATRIX_OWNER.reset(token)


def _validate_matrix_proof(root: Path, proof: SelectionMatrixProof) -> None:
    """Recheck a typed factory result; no JSON receipt import or evidence grant.

    Ownership lasts only for this validation. The artifact root is derived from
    the fixed root/profile/workspace tuple, never supplied independently, and a
    failed recheck restores the caller's ambient ownership context as well.
    """
    if not isinstance(proof, SelectionMatrixProof):
        raise ValueError("matrix validation requires a typed factory proof")
    _matrix_cells(proof.profile)
    if (
        not proof.restoration_verified
        or proof.discovery_submissions != 24
        or proof.certified_submissions != 30
        or proof.calibration_submissions != 16
        or not proof.numeric_values_verified
        or not proof.oracle_values_verified
        or proof.runtime_gpio_proven
        or proof.complete_executed_argv
        or proof.full_finding_qualified
    ):
        raise ValueError("invalid bounded matrix proof scope/restoration")
    root = root.resolve()
    source = root / selection.SOURCE
    if source.is_symlink() or not source.is_file():
        raise ValueError("missing fixed-root source ownership binding")
    token = _MATRIX_OWNER.set(_MatrixOwner(root, proof.workspace, proof.profile))
    try:
        _validate_matrix_results(
            proof.profile, proof.cells, source.read_bytes(), proof.workspace
        )
        _validate_matrix_calibrations(
            proof.profile,
            proof.calibrations,
            proof.cells,
            source.read_bytes(),
            proof.workspace,
        )
    finally:
        _MATRIX_OWNER.reset(token)


@dataclass(frozen=True, slots=True)
class _MatrixCalibrationSpec:
    name: str
    profile: _SelectionProfile
    cell: _SelectionCell
    stage: str
    category: str
    macro: str
    expected_value: int
    actual_value: int
    tu_scope: int


def _matrix_calibrations(kind: _SelectionProfile) -> tuple[_MatrixCalibrationSpec, ...]:
    _profile_spec(kind)
    cases = (
        (
            "product",
            "FASTLED_FORCE_SOFTWARE_PINS",
            0,
            1,
            _SelectionCell.FORCE0_NOMAP_ABSENT,
            STAGES[1],
        ),
        (
            "product",
            "FASTLED_FORCE_SOFTWARE_PINS",
            1,
            0,
            _SelectionCell.FORCE1_NOMAP_ABSENT,
            STAGES[1],
        ),
        ("product", "FASTLED_NO_PINMAP", 0, 1, _SelectionCell.FORCE1_NOMAP0, STAGES[1]),
        ("product", "FASTLED_NO_PINMAP", 1, 0, _SelectionCell.FORCE1_NOMAP1, STAGES[1]),
        (
            "oracle",
            "FASTLED_SELECTION_ASSERT_SCOPE",
            0,
            1,
            _SelectionCell.FORCE1_NOMAP_ABSENT,
            STAGES[1],
        ),
        (
            "oracle",
            "FASTLED_SELECTION_ASSERT_SCOPE",
            0,
            2,
            _SelectionCell.FORCE1_NOMAP_ABSENT,
            STAGES[1],
        ),
        (
            "oracle",
            "EXPECT_FORCED",
            0,
            1,
            _SelectionCell.FORCE1_NOMAP_ABSENT,
            STAGES[0],
        ),
        (
            "oracle",
            "EXPECT_NO_PINMAP",
            0,
            1,
            _SelectionCell.FORCE1_NOMAP_ABSENT,
            STAGES[0],
        ),
    )
    return tuple(
        _MatrixCalibrationSpec(
            f"{category}-{macro.lower()}-{expected}-to-{actual}-{tu}",
            kind,
            cell,
            stage,
            category,
            macro,
            expected,
            actual,
            tu,
        )
        for category, macro, expected, actual, cell, stage in cases
        for tu in (1, 2)
    )


def _require_calibration(
    kind: _SelectionProfile, control: _MatrixCalibrationSpec
) -> None:
    if not isinstance(
        control, _MatrixCalibrationSpec
    ) or control not in _matrix_calibrations(kind):
        raise ValueError("unknown closed matrix calibration profile/tuple/label")


def _matrix_literal_values(cell: _SelectionCell, name: str) -> dict[str, int | None]:
    if name not in STAGES:
        raise ValueError("unknown closed matrix source stage")
    spec = _cell_spec(cell)
    return {
        "FASTLED_FORCE_SOFTWARE_PINS": None if name == STAGES[0] else spec.force_value,
        "FASTLED_NO_PINMAP": spec.no_pinmap_value,
        "EXPECT_FORCED": int(name != STAGES[0]),
        "EXPECT_NO_PINMAP": int(spec.no_pinmap_value is not None),
        "FASTLED_SELECTION_ASSERT_SCOPE": 1
        if name == STAGES[2]
        else 2
        if name == STAGES[3]
        else 0,
    }


def _matrix_source_payloads(
    root: Path,
    kind: _SelectionProfile,
    cell: _SelectionCell,
    name: str,
    control: _MatrixCalibrationSpec | None,
) -> dict[str, bytes]:
    if cell not in _matrix_cells(kind):
        raise ValueError("unknown matrix source profile/cell")
    values = _matrix_literal_values(cell, name)
    base = producer_sources(root)
    result = {}
    for relative, tu in ((EXAMPLE, 1), (HOOK, 2)):
        body = base[relative].decode()
        # Transform only the original owned BODY. Never replace identifiers in
        # the literal prefix: doing that would create assertion tautologies.
        old_guard = (
            "#if !defined(FASTLED_SELECTION_ASSERT_SCOPE) || (FASTLED_SELECTION_ASSERT_SCOPE != 0 && FASTLED_SELECTION_ASSERT_SCOPE != 1 && FASTLED_SELECTION_ASSERT_SCOPE != 2)\n"
            '#error "closed runner assertion scope must be exactly 0, 1 or 2"\n#endif\n'
        )
        if body.count(old_guard) != 1:
            raise ValueError("owned source scope preamble changed")
        body = body.replace(old_guard, "", 1)
        replacements = {
            "#if EXPECT_FORCED\n": f"#if {values['EXPECT_FORCED']}\n",
            "#if EXPECT_NO_PINMAP\n": f"#if {values['EXPECT_NO_PINMAP']}\n",
            f"#if FASTLED_SELECTION_ASSERT_SCOPE == 0 || FASTLED_SELECTION_ASSERT_SCOPE == {tu}\n": f"#if {int(values['FASTLED_SELECTION_ASSERT_SCOPE'] in (0, tu))}\n",
        }
        for original, replacement in replacements.items():
            if body.count(original) != 1:
                raise ValueError("owned semantic source branch changed")
            body = body.replace(original, replacement, 1)
        expected = dict(values)
        if control is not None and tu != control.tu_scope:
            expected[control.macro] = control.actual_value
        prefix = (
            "// Matrix literals are independent of source semantic branch literals.\n"
        )
        for macro, value in expected.items():
            if value is None:
                prefix += f'#ifdef {macro}\n#error "matrix literal {macro} expected absent"\n#endif\n'
            else:
                prefix += (
                    f'#ifndef {macro}\n#error "matrix literal {macro} expected defined"\n#else\n'
                    f'static_assert({macro} == {value}, "matrix literal {macro} expected {value}");\n#endif\n'
                )
        result[relative] = (prefix + "// End matrix literal contract\n" + body).encode()
    return result


def _matrix_sources(
    root: Path, kind: _SelectionProfile, cell: _SelectionCell, name: str
) -> dict[str, bytes]:
    return _matrix_source_payloads(root, kind, cell, name, None)


def _matrix_calibration_sources(
    root: Path, kind: _SelectionProfile, control: _MatrixCalibrationSpec
) -> dict[str, bytes]:
    _require_calibration(kind, control)
    return _matrix_source_payloads(root, kind, control.cell, control.stage, control)


def _matrix_calibration_defines(
    kind: _SelectionProfile, control: _MatrixCalibrationSpec
) -> list[str]:
    _require_calibration(kind, control)
    original = f"-D{control.macro}={control.expected_value}"
    flags = stage_defines(control.stage, control.cell)
    if flags.count(original) != 1:
        raise ValueError("calibration does not match original closed scalar operand")
    return [
        f"-D{control.macro}={control.actual_value}" if flag == original else flag
        for flag in flags
    ]


def _validate_matrix_payloads(
    root: Path,
    kind: _SelectionProfile,
    cell: _SelectionCell,
    name: str,
    inputs: dict[str, str],
) -> None:
    for relative, payload in _matrix_sources(root, kind, cell, name).items():
        if inputs.get(relative) != hashlib.sha256(payload).hexdigest():
            raise ValueError("stale or foreign generated matrix TU payload")


def _matrix_execution_plan(kind: _SelectionProfile) -> tuple[_MatrixSubmission, ...]:
    return (
        *_matrix_submission_plan(kind),
        *(
            _MatrixSubmission(control.cell, control.name, True, control)
            for control in _matrix_calibrations(kind)
        ),
    )


def _matrix_code_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _matrix_scoped_diagnostics(
    kind: _SelectionProfile, cell: _SelectionCell, name: str
) -> set:
    sources = _matrix_sources(_matrix_code_root(), kind, cell, name)
    relative = EXAMPLE if name == STAGES[2] else HOOK
    source = "src/sketch/" + Path(relative).name
    result = set()
    for _, _, message in _scoped_diagnostics(name):
        token = message.removeprefix("static assertion failed: ")
        if message.startswith("#error"):
            token = "forced selection must not advertise hardware pin support"
        lines = [
            index
            for index, line in enumerate(sources[relative].decode().splitlines(), 1)
            if token in line
        ]
        if len(lines) != 1:
            raise ValueError("ambiguous generated matrix diagnostic source")
        result.add((source, lines[0], message))
    return result


def _assess_matrix_stage(
    kind: _SelectionProfile,
    cell: _SelectionCell,
    name: str,
    returncode: int,
    output: str,
    project: Path,
) -> SelectionOutcome:
    token = _MATRIX_DIAGNOSTICS.set(
        _matrix_scoped_diagnostics(kind, cell, name) if name in STAGES[2:4] else None
    )
    try:
        return assess_compile_result(name, returncode, output, project, cell)
    finally:
        _MATRIX_DIAGNOSTICS.reset(token)


_MATRIX_DIAGNOSTICS: ContextVar[set | None] = ContextVar(
    "matrix_diagnostics", default=None
)


def _assess_matrix_calibration(
    kind: _SelectionProfile,
    control: _MatrixCalibrationSpec,
    returncode: int,
    output: str,
    project: Path,
) -> SelectionOutcome:
    _require_calibration(kind, control)
    if type(returncode) is not int or returncode != 1 or not isinstance(output, str):
        raise ValueError(
            "calibration must be an actual compile1, never timeout/signal/success"
        )
    sources = _matrix_calibration_sources(_matrix_code_root(), kind, control)
    relative = EXAMPLE if control.tu_scope == 1 else HOOK
    message = f"matrix literal {control.macro} expected {control.expected_value}"
    lines = [
        index
        for index, line in enumerate(sources[relative].decode().splitlines(), 1)
        if f'"{message}"' in line
    ]
    if len(lines) != 1:
        raise ValueError("ambiguous owned literal diagnostic")
    expected = (
        "src/sketch/" + Path(relative).name,
        lines[0],
        "static assertion failed: " + message,
    )
    target = project / (
        "src/main.cpp"
        if control.tu_scope == 1
        else "src/sketch/AutoResearchStartupHook.cpp"
    )
    envelope = f"build error: build failed: compilation failed for {target}:"
    observed = False
    summaries = 0
    for line in output.splitlines():
        if line == envelope:
            summaries += 1
            continue
        if not re.search(
            r"\b(?:fatal\s+)?error:|AddressSanitizer|UndefinedBehaviorSanitizer", line
        ):
            continue
        match = re.fullmatch(
            r"(src/sketch/(?:AutoResearch\.ino|AutoResearchStartupHook\.cpp)):(\d+):(\d+): error: (.+)",
            line,
        )
        if (
            match is None
            or int(match[3]) < 1
            or (match[1], int(match[2]), match[4]) != expected
        ):
            raise ValueError("unbound calibration diagnostic/project/TU/line/family")
        observed = True
    if not summaries or not observed:
        raise ValueError("missing selected calibration assertion or wrapper envelope")
    return SelectionOutcome(1, True, False, False, cell=control.cell)


def _run_matrix_calibration_compile(
    workspace: Path,
    saved: Path,
    environment: dict,
    kind: _SelectionProfile,
    control: _MatrixCalibrationSpec,
):
    flags = _matrix_calibration_defines(kind, control)
    saved.mkdir(parents=True, exist_ok=True)
    command = [
        "bash",
        "compile",
        _profile_spec(kind).env,
        "--examples",
        "AutoResearch",
        "--defines",
        ",".join(flag.removeprefix("-D") for flag in flags),
        "-v",
        "--build-info",
        str(saved / "build-info.json"),
    ]
    vendor._json(
        saved / "invocation.json",
        {"argv": command, "cwd": workspace, "complete_executed_argv": False},
    )
    return native._run(
        command, workspace, saved / "build.log", build=True, environment=environment
    )


def _bound_producer(workspace: Path, inputs: dict[str, str]) -> bytes:
    path = workspace / EXAMPLE
    if path.is_symlink() or not path.is_file():
        raise ValueError("missing controlled producer")
    content = path.read_bytes()
    if hashlib.sha256(content).hexdigest() != inputs.get(EXAMPLE):
        raise ValueError("producer differs from frozen stage source")
    return content


def _install_matrix_sources(
    workspace: Path,
    kind: _SelectionProfile,
    cell: _SelectionCell,
    name: str,
    initial: dict[str, str],
) -> dict[str, str]:
    if _SELECTION_CONTEXT.get() is not _matrix_context(kind):
        return dict(initial)
    expected = dict(initial)
    for relative, content in _matrix_sources(workspace, kind, cell, name).items():
        path = workspace / relative
        if path.is_symlink() or not path.is_file():
            raise ValueError("unsafe matrix stage producer")
        path.write_bytes(content)
        expected[relative] = hashlib.sha256(content).hexdigest()
    return expected


@dataclass(frozen=True, slots=True)
class _MatrixRuntime:
    info: dict
    environment: dict
    tools: dict
    roots: tuple
    profile: dict
    engine: dict
    configuration: dict[str, bytes]


_MATRIX_RUNTIME: ContextVar[_MatrixRuntime | None] = ContextVar(
    "matrix_runtime", default=None
)


@dataclass(frozen=True, slots=True)
class SelectionMatrixCalibration:
    control: _MatrixCalibrationSpec
    stage: SelectionStage
    prior_configuration_sha256s: dict[str, str] = field(default_factory=dict)
    post_configuration_sha256s: dict[str, str] = field(default_factory=dict)


def _validate_calibration_tokens(
    kind: _SelectionProfile,
    control: _MatrixCalibrationSpec,
    tokens: list[str],
    *,
    partial: bool = False,
) -> None:
    _require_calibration(kind, control)
    if not isinstance(tokens, list) or any(
        not isinstance(token, str) for token in tokens
    ):
        raise ValueError("nonstring calibration configuration operand")
    checked = list(tokens)
    if partial:
        present = set()
        for index, token in enumerate(tokens):
            operand = token[2:] if token.startswith("-D") else ""
            if token == "-D" and index + 1 < len(tokens):
                operand = tokens[index + 1]
            match = re.match(r"[A-Za-z_]\w*", operand)
            if match:
                present.add(match[0])
        checked += [
            flag
            for flag in _matrix_calibration_defines(kind, control)
            if flag[2:].partition("=")[0] not in present
        ]
    actual = f"{control.macro}={control.actual_value}"
    expected = f"{control.macro}={control.expected_value}"
    replacements = 0
    for index, token in enumerate(checked):
        if token == "-D" + actual:
            checked[index] = "-D" + expected
            replacements += 1
        elif index and checked[index - 1] == "-D" and token == actual:
            checked[index] = expected
            replacements += 1
    if replacements != 1:
        raise ValueError("missing/duplicate actual calibration scalar operand")
    validate_stage_defines(control.stage, checked, control.cell)


def _capture_calibration_configuration(
    info: dict, kind: _SelectionProfile, control: _MatrixCalibrationSpec
) -> CellConfiguration:
    _require_calibration(kind, control)
    project = Path(info["prog_path"]).parents[3]
    if project.name != _profile_spec(kind).env:
        raise ValueError("calibration configuration project/profile mismatch")
    ini = project / "platformio.ini"
    if ini.is_symlink() or not ini.is_file():
        raise ValueError("missing actual calibration INI")
    parser = configparser.ConfigParser(
        interpolation=None, inline_comment_prefixes=("#", ";"), strict=True
    )
    parser.read_string(ini.read_text())
    selected = "env:" + _profile_spec(kind).env
    if parser.defaults() or not parser.has_section(selected):
        raise ValueError("unbound calibration selected environment")
    flags = shlex.split(parser.get(selected, "build_flags", raw=True), comments=True)
    _validate_calibration_tokens(kind, control, flags)
    # An actual failed compilation cannot provide a fresh database receipt.
    # Prior and post-failure DB/rsp bytes are retained separately below, with
    # explicit unproven freshness and only controlled recipe transitions.
    return CellConfiguration(
        control.name,
        {str(ini): vendor._digest(ini)},
        {str(ini): ini.read_bytes()},
        {},
        control.cell,
    )


def _matrix_calibration_state(
    info: dict,
    environment: dict,
    tools: dict,
    roots: tuple,
    project: Path,
    kind: _SelectionProfile,
    control: _MatrixCalibrationSpec,
) -> dict:
    _require_calibration(kind, control)
    _validate_info_profile(info, kind)
    return vendor._base_state(info, environment, tools, roots, project)


def _configuration_bytes_after(prior: dict[str, bytes]) -> dict[str, bytes]:
    result = {}
    for filename in prior:
        path = Path(filename)
        if path.is_symlink() or not path.is_file():
            raise ValueError("missing or unsafe known calibration recipe input")
        result[filename] = path.read_bytes()
    # The factory's ordinary capture always includes one INI and the complete
    # project DB/rsp universe. No arbitrary file additions are permitted later.
    inis = [Path(name) for name in prior if Path(name).name == "platformio.ini"]
    for ini in inis:
        known = {
            str(ini),
            *(str(path) for path in ini.parent.rglob("*.rsp")),
            *(str(path) for path in ini.parent.rglob("compile_commands.raw.json")),
        }
        if known != set(prior):
            raise ValueError("calibration changed the fixed recipe universe")
    return result


def _matrix_recipe_transition(
    prior: dict[str, bytes],
    after: dict[str, bytes],
    kind: _SelectionProfile,
    control: _MatrixCalibrationSpec,
) -> None:
    if set(prior) != set(after):
        raise ValueError("calibration recipe file universe drift")

    def expand(
        tokens: list[str],
        directory: Path,
        files: dict[str, bytes],
        active: frozenset[Path] = frozenset(),
    ) -> list[str]:
        result = []
        for token in tokens:
            if not token.startswith("@"):
                result.append(token)
                continue
            path = (directory / token[1:]).resolve()
            if path in active or str(path) not in files:
                raise ValueError("unbound/external calibration response reference")
            result += expand(
                shlex.split(files[str(path)].decode()),
                directory,
                files,
                active | {path},
            )
        return result

    for filename in prior:
        if prior[filename] == after[filename]:
            continue  # Known prior input, explicitly NOT fresh current argv.
        path = Path(filename)
        if path.name == "platformio.ini":
            normalized = []
            for phase, content in enumerate((prior[filename], after[filename])):
                parser = configparser.ConfigParser(
                    interpolation=None, inline_comment_prefixes=("#", ";"), strict=True
                )
                parser.read_string(content.decode())
                selected = "env:" + _profile_spec(kind).env
                if parser.defaults() or not parser.has_section(selected):
                    raise ValueError("calibration environment drift")
                values: dict[str, dict[str, str | list[str]]] = {
                    name: dict(parser.items(name, raw=True))
                    for name in parser.sections()
                }
                flags = shlex.split(
                    parser.get(selected, "build_flags", raw=True), comments=True
                )
                if phase == 1:
                    _validate_calibration_tokens(kind, control, flags)
                values[selected]["build_flags"] = _matrix_strip_definitions(flags)
                normalized.append(values)
        elif path.name == "compile_commands.raw.json":
            normalized = []
            for files in (prior, after):
                rows = []
                for row in json.loads(files[filename]):
                    item = dict(row)
                    tokens = row.get("arguments")
                    if tokens is None:
                        tokens = shlex.split(row["command"])
                    elif "command" in row and tokens != shlex.split(row["command"]):
                        raise ValueError("contradictory calibration planned recipe")
                    tokens = expand(tokens, Path(row["directory"]), files)
                    if files is after:
                        _validate_calibration_tokens(kind, control, tokens)
                    item.pop("command", None)
                    item["arguments"] = _matrix_strip_definitions(tokens)
                    rows.append(item)
                normalized.append(rows)
        elif path.suffix == ".rsp":
            current = shlex.split(after[filename].decode())
            _validate_calibration_tokens(kind, control, current, partial=True)
            normalized = [
                _matrix_strip_definitions(shlex.split(prior[filename].decode())),
                _matrix_strip_definitions(current),
            ]
        else:
            raise ValueError("unreviewed calibration recipe file")
        if normalized[0] != normalized[1]:
            raise ValueError("calibration recipe changed beyond fixed scalar macros")


def _calibration_configuration_binding(
    stage: SelectionStage, kind: _SelectionProfile, control: _MatrixCalibrationSpec
) -> None:
    sources = _matrix_configuration_sources(stage)
    inis = 0
    for source, retained in sources.items():
        if source.name.endswith("platformio.ini"):
            parser = configparser.ConfigParser(
                interpolation=None, inline_comment_prefixes=("#", ";"), strict=True
            )
            parser.read_string(retained.read_text())
            selected = "env:" + _profile_spec(kind).env
            if parser.defaults() or not parser.has_section(selected):
                raise ValueError("foreign calibration environment/profile")
            _validate_calibration_tokens(
                kind,
                control,
                shlex.split(
                    parser.get(selected, "build_flags", raw=True), comments=True
                ),
            )
            inis += 1
        elif source.name.endswith("compile_commands.raw.json"):
            rows = json.loads(retained.read_text())
            if not isinstance(rows, list) or not rows:
                raise ValueError("missing conditional calibration recipe rows")
            for row in rows:
                tokens = row.get("arguments") or shlex.split(row["command"])
                if (
                    "arguments" in row
                    and "command" in row
                    and tokens != shlex.split(row["command"])
                ):
                    raise ValueError("contradictory calibration recipe")
                _validate_calibration_tokens(kind, control, tokens)
        elif source.suffix == ".rsp":
            _validate_calibration_tokens(
                kind, control, shlex.split(retained.read_text()), partial=True
            )
        else:
            raise ValueError("unreviewed retained calibration configuration")
    if inis != 1:
        raise ValueError("missing unique calibration INI binding")


def _validate_matrix_calibration_record(
    kind: _SelectionProfile,
    record: SelectionMatrixCalibration,
    original_source: bytes,
    workspace: Path,
) -> None:
    if not isinstance(record, SelectionMatrixCalibration):
        raise ValueError("untyped calibration record")
    control, stage = record.control, record.stage
    _require_calibration(kind, control)
    owner = _matrix_owner(kind, workspace)
    directory = (
        _matrix_owned_artifacts(owner)
        / control.cell.value
        / "calibrations"
        / control.name
    )
    if (
        not isinstance(stage, SelectionStage)
        or stage.name != control.name
        or stage.cell is not control.cell
    ):
        raise ValueError("calibration stage tuple mismatch")
    if stage.project != workspace / ".build/fbuild" / _profile_spec(kind).env:
        raise ValueError("calibration architecture/project mismatch")
    for before, after in (
        (stage.inputs_before, stage.inputs_after),
        (stage.staged_sources_expected, stage.staged_sources_observed),
        (stage.tools_before, stage.tools_after),
        (stage.profile_before, stage.profile_after),
        (stage.serving_engine_before, stage.serving_engine_after),
    ):
        if not before or before != after:
            raise ValueError("missing or changed calibration build binding")
    if (
        not _REQUIRED_SOURCES <= stage.inputs_before.keys()
        or stage.staged_sources_expected != _staged_subset(stage.inputs_before)
    ):
        raise ValueError("missing calibration source/TU universe")
    _verify_files({str(stage.log_path): stage.log_sha256})
    assessed = _assess_matrix_calibration(
        kind,
        control,
        stage.outcome.returncode,
        stage.log_path.read_text(),
        stage.project,
    )
    if assessed != stage.outcome:
        raise ValueError("calibration outcome/freshness mismatch")
    for filename in (
        *stage.artifact_sha256s,
        *stage.configuration_sha256s,
        *record.prior_configuration_sha256s,
        *record.post_configuration_sha256s,
        str(stage.log_path),
    ):
        _require_matrix_owned_file(Path(filename), directory, owner)
    if any(
        Path(name).name in {"firmware.elf", "firmware.bin", "compile_commands.raw.json"}
        for name in stage.artifact_sha256s
    ):
        raise ValueError("calibration compile1 cannot claim fresh ELF/database")
    _verify_files(stage.artifact_sha256s)
    _verify_files(record.prior_configuration_sha256s)
    _verify_files(record.post_configuration_sha256s)
    _calibration_configuration_binding(stage, kind, control)
    expected = {
        **_matrix_calibration_sources(_matrix_code_root(), kind, control),
        selection.SOURCE: original_source,
    }
    for relative, payload in expected.items():
        sha = hashlib.sha256(payload).hexdigest()
        for values in (
            stage.inputs_before,
            stage.inputs_after,
            stage.staged_sources_expected,
            stage.staged_sources_observed,
        ):
            if values.get(relative) != sha:
                raise ValueError(
                    "calibration payload is not the exact code-owned source"
                )
        retained = directory / "staged-sources" / relative
        if stage.artifact_sha256s.get(str(retained)) != sha:
            raise ValueError("calibration actual payload copy is unbound")


def _validate_matrix_calibrations(
    kind: _SelectionProfile,
    records: list[SelectionMatrixCalibration],
    ordinary: list[SelectionMatrixCell],
    original_source: bytes,
    workspace: Path,
) -> None:
    expected = _matrix_calibrations(kind)
    if len(records) != 16 or any(
        not isinstance(record, SelectionMatrixCalibration) for record in records
    ):
        raise ValueError("missing typed calibration coverage")
    if tuple(record.control for record in records) != expected:
        raise ValueError("missing/duplicate/foreign calibration tuple coverage")
    first = ordinary[0].stages[0]
    for record in records:
        _validate_matrix_calibration_record(kind, record, original_source, workspace)
        stage = record.stage
        if stage.tools_before != first.tools_before:
            raise ValueError("calibration tool drift")
        for key in ("tools", "sdk", "environment", "configuration"):
            if stage.profile_before.get(key) != first.profile_before.get(key):
                raise ValueError("calibration immutable base profile drift")
        if vendor._engine_semantics(
            stage.serving_engine_before
        ) != vendor._engine_semantics(first.serving_engine_before):
            raise ValueError("calibration serving semantics drift")
        controlled = {selection.SOURCE, EXAMPLE, HOOK}
        if {
            key: value
            for key, value in stage.inputs_before.items()
            if key not in controlled
        } != {
            key: value
            for key, value in first.inputs_before.items()
            if key not in controlled
        }:
            raise ValueError("uncontrolled calibration source/input drift")


def _execute_matrix_calibrations(
    root: Path,
    workspace: Path,
    artifacts: Path,
    initial: dict,
    original_inputs: dict,
    original_source: bytes,
    kind: _SelectionProfile,
    ordinary: list[SelectionMatrixCell],
) -> list[SelectionMatrixCalibration]:
    runtime = _MATRIX_RUNTIME.get()
    if not isinstance(runtime, _MatrixRuntime):
        raise ValueError("missing actual ordinary-build context for calibration")
    context = _SELECTION_CONTEXT.set(_matrix_context(kind))
    records = []
    try:
        for control in _matrix_calibrations(kind):
            _check_matrix_deadline(reserve_build=True)
            saved = artifacts / control.cell.value / "calibrations" / control.name
            saved.mkdir(parents=True, exist_ok=True)
            native._verify_inputs(root, original_inputs)
            prior = _configuration_bytes_after(runtime.configuration)
            if prior != runtime.configuration:
                raise ValueError("known prior recipe changed between completed builds")
            expected = dict(initial)
            payloads = _matrix_calibration_sources(workspace, kind, control)
            for relative, content in {
                **payloads,
                selection.SOURCE: original_source,
            }.items():
                path = workspace / relative
                if path.is_symlink() or not path.is_file():
                    raise ValueError("unsafe controlled calibration source")
                path.write_bytes(content)
                expected[relative] = hashlib.sha256(content).hexdigest()
            native._verify_inputs(workspace, expected)
            project = workspace / ".build/fbuild" / _profile_spec(kind).env
            before_static = _matrix_calibration_state(
                runtime.info,
                runtime.environment,
                runtime.tools,
                runtime.roots,
                project,
                kind,
                control,
            )
            frozen_base = {
                key: runtime.profile[key]
                for key in ("tools", "sdk", "environment", "configuration")
            }
            vendor.property_check._verify_profile_state(
                saved, "calibration-before", frozen_base, before_static
            )
            # Raw prior files are frozen INPUTS, not a claim about the failed
            # invocation's fresh database or complete executed argv.
            prior_hashes = _retain_cell(
                saved,
                CellConfiguration(
                    control.name,
                    {
                        name: hashlib.sha256(data).hexdigest()
                        for name, data in prior.items()
                    },
                    prior,
                    {},
                    control.cell,
                ),
                "prior-input-configuration",
            )
            window = _compile_engine_window(
                workspace,
                saved,
                runtime.environment,
                runtime.tools,
                runtime.engine,
                control.name,
                kind,
                control.cell,
                calibration=control,
            )
            after_static = _matrix_calibration_state(
                runtime.info,
                runtime.environment,
                runtime.tools,
                runtime.roots,
                project,
                kind,
                control,
            )
            vendor.property_check._verify_profile_state(
                saved, "calibration-after", frozen_base, after_static
            )
            native._verify_inputs(root, original_inputs)
            native._verify_inputs(workspace, expected)
            vendor._check_staged_sources(
                workspace, expected, project, _bound_producer(workspace, expected)
            )
            outcome = _assess_matrix_calibration(
                kind,
                control,
                window.compile_result.returncode,
                window.compile_result.stdout,
                project,
            )
            after = _configuration_bytes_after(prior)
            _matrix_recipe_transition(prior, after, kind, control)
            configuration = _capture_calibration_configuration(
                runtime.info, kind, control
            )
            retained_configuration = _retain_cell(
                saved, configuration, "actual-ini-configuration"
            )
            post_hashes = _retain_cell(
                saved,
                CellConfiguration(
                    control.name,
                    {
                        name: hashlib.sha256(data).hexdigest()
                        for name, data in after.items()
                    },
                    after,
                    {},
                    control.cell,
                ),
                "post-failure-unqualified-configuration",
            )
            copied_sources = {}
            for relative in _REQUIRED_SOURCES:
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
                    raise ValueError("actual calibration staged payload mismatch")
                target = saved / "staged-sources" / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(staged.read_bytes())
                copied_sources[str(target)] = vendor._digest(target)
            bindings = _observe_staged_inputs(workspace, expected, project)
            log = saved / "build.log"
            stage = SelectionStage(
                control.name,
                project,
                outcome,
                log,
                vendor._digest(log),
                dict(expected),
                dict(expected),
                bindings.expected,
                bindings.observed,
                before_static["tools"],
                after_static["tools"],
                before_static,
                after_static,
                window.before,
                window.after,
                copied_sources,
                retained_configuration,
                control.cell,
            )
            record = SelectionMatrixCalibration(
                control, stage, prior_hashes, post_hashes
            )
            _validate_matrix_calibration_record(
                kind, record, original_source, workspace
            )
            vendor._json(saved / "calibration.json", asdict(record))
            vendor._json(
                saved / "actual-build-result.json",
                {
                    "returncode": outcome.returncode,
                    "expected_compile_failure": True,
                    "fresh_elf": False,
                    "fresh_database": False,
                    "complete_executed_argv": False,
                },
            )
            records.append(record)
            runtime = _MatrixRuntime(
                runtime.info,
                runtime.environment,
                runtime.tools,
                runtime.roots,
                {
                    **frozen_base,
                    "planned_configuration": {
                        name: hashlib.sha256(data).hexdigest()
                        for name, data in after.items()
                    },
                },
                window.after,
                after,
            )
            _MATRIX_RUNTIME.set(runtime)
    finally:
        _SELECTION_CONTEXT.reset(context)
    return records
