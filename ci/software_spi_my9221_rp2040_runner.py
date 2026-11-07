"""Fixed three-build RP2040 property attempt; never imports external proof data.

Only potential direct-controller reachability and whole-emitter SRAM placement
are qualified. Raw compile databases are planned arguments, not executed receipts.
"""

import hashlib
import json
import os
import re
import shutil
import struct
import sys
from dataclasses import asdict, dataclass
from enum import Enum
from pathlib import Path

from ci import software_spi_esp32_c6_mode_proof as mode_check
from ci import software_spi_esp32_xtensa_mode_proof as xtensa_check
from ci import software_spi_init_proof as native
from ci import software_spi_my9221_rp2040_irq_proof as irq_check
from ci import software_spi_my9221_rp2040_proof as property_check


EXAMPLE = "examples/AutoResearch/AutoResearch.ino"
PRODUCER = b'#include <FastLED.h>\n\n// Fixed direct public-controller producer; no FastLED global-list dispatch claim.\nstatic CRGB my9221_pixels[4];\nvolatile fl::u8 my9221_input = 0x30;\n\nextern "C" __attribute__((noinline, used)) void my9221_rp2040_public_producer() {\n    MY9221Controller<2, 3, RGB> my9221_controller;\n    my9221_pixels[0] = CRGB(my9221_input, 0x80, 0x01);\n    my9221_pixels[1] = CRGB(0x01, 0x02, 0x04);\n    my9221_pixels[2] = CRGB(0x08, 0x10, 0x20);\n    my9221_pixels[3] = CRGB(0x40, 0x80, 0x30);\n    my9221_controller.init();\n    my9221_controller.setLeds(my9221_pixels, 4);\n    my9221_controller.showLeds(255);\n}\n\nvoid setup() { my9221_rp2040_public_producer(); }\nvoid loop() {}\n'


class _PropertyKind(Enum):
    ESP32_XTENSA_MODE = "esp32-xtensa-mode"
    PLACEMENT = "placement"
    IRQ = "irq"
    ESP32_C6_MODE = "esp32-c6-mode"


@dataclass(frozen=True)
class _VendorSpec:
    source: str
    mutant: str
    scope: str
    snapshot_kind: native._ProofKind
    artifact_group: str


def _vendor_spec(kind: _PropertyKind) -> _VendorSpec:
    if kind is _PropertyKind.ESP32_XTENSA_MODE:
        return _VendorSpec(
            xtensa_check.SOURCE,
            xtensa_check.STAGES[1],
            "esp32-xtensa-forced-gpio2-3-conditional-output-mode-property-only",
            native._ProofKind.ESP32_XTENSA_MODE,
            "spi-esp32-xtensa-mode-proof",
        )
    if kind is _PropertyKind.ESP32_C6_MODE:
        return _VendorSpec(
            mode_check.SOURCE,
            mode_check.STAGES[1],
            "esp32c6-forced-gpio2-3-output-readback-mode-property-only",
            native._ProofKind.ESP32_C6_MODE,
            "spi-esp32-c6-mode-proof",
        )
    if kind is _PropertyKind.PLACEMENT:
        return _VendorSpec(
            property_check.SOURCE,
            "kernel-xip-mutant",
            "rp2040-native-gpio2-3-elf-sram-property-only",
            native._ProofKind.RP2040_ELF,
            "spi-my9221-rp2040-elf-proof",
        )
    if kind is _PropertyKind.IRQ:
        return _VendorSpec(
            irq_check.SOURCE,
            "unconditional-enable-mutant",
            "rp2040-native-gpio2-3-saved-primask-property-only",
            native._ProofKind.RP2040_IRQ,
            "spi-my9221-rp2040-irq-proof",
        )
    raise ValueError("unknown closed vendor property")


@dataclass(frozen=True)
class _Assessment:
    errors: list[str]
    irq_analysis: irq_check.IRQResult | None = None
    mode_analysis: mode_check.ModeResult | None = None
    xtensa_analysis: xtensa_check.ModeResult | None = None


def _assess_property(capture: dict, kind: _PropertyKind, stage: str) -> _Assessment:
    spec = _vendor_spec(kind)
    if stage not in ("baseline", spec.mutant, "restored"):
        raise ValueError("unknown fixed vendor stage")
    if kind is _PropertyKind.ESP32_XTENSA_MODE:
        result = xtensa_check.classify_capture(capture)
        xtensa_check.validate_stage_analysis(stage, result)
        return _Assessment(result.errors, xtensa_analysis=result)
    if kind is _PropertyKind.IRQ:
        result = irq_check.classify_irq_capture(capture)
        irq_check.validate_stage_analysis(stage, result)
        return _Assessment(result.errors, result)
    if kind is _PropertyKind.ESP32_C6_MODE:
        result = mode_check.classify_capture(capture)
        mode_check.validate_stage_analysis(stage, result)
        return _Assessment(result.errors, mode_analysis=result)
    errors = property_check.classify_capture(capture)
    intended = ["kernel-not-in-sdk-sram"] if stage == spec.mutant else []
    if errors != intended:
        raise ValueError(f"RP2040 stage lacks intended bounded ELF property: {errors}")
    return _Assessment(errors)


@dataclass(frozen=True, slots=True)
class _StagePropertyFlags:
    public_producer_reachable: bool
    callback_arguments_resolved: bool
    masked_callee_literal_closure: bool


def _stage_property_flags(
    kind: _PropertyKind, assessment: _Assessment
) -> _StagePropertyFlags:
    if kind in (_PropertyKind.ESP32_C6_MODE, _PropertyKind.ESP32_XTENSA_MODE):
        return _StagePropertyFlags(False, False, False)
    return _StagePropertyFlags(
        True, True, assessment.irq_analysis is not None or not assessment.errors
    )


def _mode_helper(kind: _PropertyKind):
    if kind is _PropertyKind.ESP32_XTENSA_MODE:
        return xtensa_check
    if kind is _PropertyKind.ESP32_C6_MODE:
        return mode_check
    raise ValueError("not a closed ESP mode kind")


def _is_mode(kind: _PropertyKind) -> bool:
    return kind in (_PropertyKind.ESP32_C6_MODE, _PropertyKind.ESP32_XTENSA_MODE)


@dataclass(frozen=True)
class VendorStage:
    name: str
    build_returncode: int
    inputs_before: dict[str, str]
    inputs_after: dict[str, str]
    tools_before: dict
    tools_after: dict
    configuration: dict
    raw_database_scope: str
    artifact_sha256s: dict[str, str]
    property_errors: list[str]
    public_producer_reachable: bool
    callback_arguments_resolved: bool
    masked_callee_literal_closure: bool
    serving_engine_before: dict
    serving_engine_after: dict
    irq_analysis: irq_check.IRQResult | None = None
    mode_analysis: mode_check.ModeResult | None = None
    xtensa_analysis: xtensa_check.ModeResult | None = None


@dataclass(frozen=True)
class VendorProof:
    scope: str
    public_api_scope: str
    global_fastled_dispatch_proven: bool
    complete_executed_argv: bool
    workspace: Path
    stages: list[VendorStage]
    bootstrap: Path
    engine_profile: dict
    limitation: str


def _digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _json(path: Path, data: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(data, default=str, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


# Explicit reviewed path/cache/tool/flags/locale inputs only. No arbitrary
# inherited values or token/proxy-prefix discovery; never fingerprint secrets.
CHILD_ENVIRONMENT_KEYS = frozenset(
    (
        "HOME",
        "USERPROFILE",
        "PATH",
        "PATHEXT",
        "SYSTEMROOT",
        "WINDIR",
        "COMSPEC",
        "TEMP",
        "TMP",
        "TMPDIR",
        "APPDATA",
        "LOCALAPPDATA",
        "XDG_CACHE_HOME",
        "XDG_CONFIG_HOME",
        "XDG_DATA_HOME",
        "LANG",
        "LC_ALL",
        "LC_CTYPE",
        "TZ",
        "TERM",
        "CC",
        "CXX",
        "AR",
        "AS",
        "LD",
        "CFLAGS",
        "CXXFLAGS",
        "CPPFLAGS",
        "LDFLAGS",
        "GCC_EXEC_PREFIX",
        "COMPILER_PATH",
        "CPATH",
        "C_INCLUDE_PATH",
        "CPLUS_INCLUDE_PATH",
        "LIBRARY_PATH",
        "LD_LIBRARY_PATH",
        "DYLD_LIBRARY_PATH",
        "SDKROOT",
        "UV_CACHE_DIR",
        "UV_PYTHON_INSTALL_DIR",
        "UV_TOOL_DIR",
        "FBUILD_DAEMON_PORT",
        "FBUILD_CACHE_DIR",
        "FBUILD_HOME",
        "ZCCACHE_DIR",
        "CCACHE_DIR",
        "SCCACHE_DIR",
    )
)


STARTUP_ENVIRONMENT_KEYS = CHILD_ENVIRONMENT_KEYS | frozenset(
    (
        "UV_NO_SYNC",
        "UV_PROJECT_ENVIRONMENT",
        "VIRTUAL_ENV",
        "PYTHONPATH",
        "PYTHONNOUSERSITE",
        "PYTHONDONTWRITEBYTECODE",
        "FBUILD_DEV_MODE",
        "FBUILD_NO_UPDATE_CHECK",
        "FBUILD_EXECUTABLE",
        "FBUILD_SELF_EVICTION_SECS",
    )
)


def _startup_environment_hashes(raw: bytes) -> dict[str, str]:
    """Hash reviewed server startup keys only, never unknown values/secrets."""
    result = {}
    for entry in raw.split(b"\0"):
        key, separator, value = entry.partition(b"=")
        if not separator:
            continue
        name = key.decode("ascii", errors="ignore")
        if name not in STARTUP_ENVIRONMENT_KEYS or key != name.encode("ascii"):
            continue
        if name in result:
            raise ValueError("ambiguous reviewed daemon startup environment key")
        result[name] = hashlib.sha256(value).hexdigest()
    return result


def _environment(workspace: Path) -> dict[str, str]:
    env = {
        key: value for key, value in os.environ.items() if key in CHILD_ENVIRONMENT_KEYS
    }
    env.update(
        UV_NO_SYNC="1",
        UV_PROJECT_ENVIRONMENT=sys.prefix,
        VIRTUAL_ENV=sys.prefix,
        PYTHONPATH=str(workspace.resolve()),
        PYTHONNOUSERSITE="1",
        PYTHONDONTWRITEBYTECODE="1",
        FBUILD_DEV_MODE="1",
        FBUILD_NO_UPDATE_CHECK="1",
        FBUILD_EXECUTABLE=str(Path(sys.prefix) / "bin/fbuild"),
    )
    env["PATH"] = (
        str(Path(sys.prefix) / "bin") + os.pathsep + env.get("PATH", os.defpath)
    )
    return env


def _compile(workspace: Path, saved: Path, environment: dict[str, str]) -> dict:
    command = [
        "bash",
        "compile",
        "rp2040",
        "--examples",
        "AutoResearch",
        "-v",
        "--build-info",
        str(saved / "build-info.json"),
    ]
    _json(
        saved / "invocation.json",
        {"argv": command, "cwd": workspace, "complete_executed_argv": False},
    )
    result = native._run(
        command, workspace, saved / "build.log", build=True, environment=environment
    )
    if result.returncode != 0:
        raise ValueError(f"RP2040 vendor build failed; retained {saved}")
    route = [
        str(Path(environment["FBUILD_EXECUTABLE"]).resolve()),
        str(workspace / ".build/fbuild/rp2040"),
        "build",
        "-e",
        "rp2040",
        "-v",
    ]
    observed = "Running: " + " ".join(route)
    _json(
        saved / "build-route.json",
        {
            "wrapper_observed_cli_argv": route,
            "observed_in_log": observed in result.stdout,
            "complete_executed_argv": False,
        },
    )
    if observed not in result.stdout:
        raise ValueError("unqualified actual fbuild single-example submission route")
    info = json.loads((saved / "build-info.json").read_text())["rp2040"]
    if (
        info["board"] != "RPIPICO"
        or info["platform"] != "RP2040"
        or "-DF_CPU=133000000L" not in info["cxx_flags"]
    ):
        raise ValueError("unsupported fixed RP2040 SDK/CPU profile")
    if any("FASTLED_FORCE_SOFTWARE_PINS" in flag for flag in info["cxx_flags"]):
        raise ValueError("RP2040 native profile cannot certify forced pin provider")
    return info


def _board(kind: _PropertyKind) -> str:
    if kind is _PropertyKind.ESP32_XTENSA_MODE:
        return "esp32dev"
    return "esp32c6" if kind is _PropertyKind.ESP32_C6_MODE else "rp2040"


def _compile_selected(
    workspace: Path, saved: Path, environment: dict, kind: _PropertyKind
) -> dict:
    if _is_mode(kind):
        return _mode_helper(kind).compile_fixed(workspace, saved, environment)
    return _compile(workspace, saved, environment)


def _profile_roots(info: dict) -> tuple[Path, Path]:
    if info.get("env") == "esp32dev":
        return xtensa_check.sdk_roots(info)
    if info.get("env") == "esp32c6":
        return mode_check.sdk_roots(info)
    roots = {
        Path(path).parent.parent
        for path in info["includes"]
        if path.endswith("/cores/rp2040")
    }
    if len(roots) != 1:
        raise ValueError("ambiguous selected RP2040 SDK")
    sdk = roots.pop().resolve()
    if (
        sdk.name != "rp2040-5.7.0"
        or not (sdk / "lib/rp2040/memmap_default.ld").is_file()
    ):
        raise ValueError("unreviewed RP2040 SDK recipe")
    return sdk, Path(info["cc_path"]).resolve().parents[2]


def _tree_hashes(roots: tuple[Path, ...]) -> dict[str, str]:
    result = {}
    for root in roots:
        if not root.is_dir():
            raise ValueError(f"missing provisioned dependency root {root}")
        for directory, dirs, files in os.walk(root, followlinks=False):
            base = Path(directory)
            if any((base / name).is_symlink() for name in dirs):
                raise ValueError("unqualified SDK directory symlink")
            for name in files:
                path = base / name
                if not path.is_file():
                    raise ValueError("unsafe SDK dependency")
                result[str(path)] = _digest(path)
    return result


def _tool_identities(
    info: dict, workspace: Path, environment: dict[str, str], saved: Path
) -> dict[str, native.ToolIdentity]:
    paths = {
        name: Path(info[key])
        for name, key in (
            ("compiler", "cc_path"),
            ("cxx", "cxx_path"),
            ("archive", "ar_path"),
            ("objdump", "objdump_path"),
            ("readelf", "readelf_path"),
            ("nm", "nm_path"),
        )
    }
    paths["linker"] = Path(info["aliases"]["ld"])
    paths["fbuild"] = Path(sys.prefix) / "bin/fbuild"
    paths["fbuild-daemon"] = Path(sys.prefix) / "bin/fbuild-daemon"
    paths["python"] = Path(sys.executable).absolute()
    if paths["fbuild-daemon"].absolute() != paths["python"].parent / "fbuild-daemon":
        raise ValueError(
            "selected venv Python/adjacent daemon mismatch; PATH fallback prohibited"
        )
    if not paths["fbuild-daemon"].is_file():
        raise ValueError(
            "selected venv adjacent daemon missing; PATH fallback prohibited"
        )
    for name in ("bash", "uv"):
        selected = shutil.which(name, path=environment["PATH"])
        if selected is None:
            raise ValueError(f"missing managed wrapper tool {name}")
        paths[name] = Path(selected)
    # The installed native extension also participates in the managed engine.
    from importlib.util import find_spec

    spec = find_spec("fbuild")
    if spec is None or spec.submodule_search_locations is None:
        raise ValueError("missing provisioned fbuild package")
    extensions = list(
        Path(next(iter(spec.submodule_search_locations))).glob("_native*.so")
    )
    if len(extensions) != 1:
        raise ValueError("unqualified fbuild native extension")
    tools = {}
    for name, path in paths.items():
        before = _digest(path)
        if name == "fbuild-daemon":
            # The service has no --version CLI. This temporary identity only
            # supplies the expected disk SHA to the verified running-image probe.
            tools[name] = native.ToolIdentity(
                path.absolute(), before, "awaiting verified health"
            )
            continue
        result = native._run(
            [str(path.resolve(strict=True)), "--version"],
            workspace,
            saved / f"tool-{name}.log",
            build=False,
            environment=environment,
        )
        if (
            result.returncode != 0
            or not result.stdout.strip()
            or _digest(path) != before
        ):
            raise ValueError(
                f"actual vendor tool identity unavailable or changed: {name}"
            )
        tools[name] = native.ToolIdentity(
            path.absolute(), before, result.stdout.strip()
        )
    extension = extensions[0].absolute()
    tools["fbuild-extension"] = native.ToolIdentity(
        extension, _digest(extension), _digest(extension)
    )
    serving = _daemon_identity(workspace, saved, "tool-discovery", environment, tools)
    daemon = tools["fbuild-daemon"]
    tools["fbuild-daemon"] = native.ToolIdentity(
        daemon.path, daemon.sha256, f"{serving['version']} ({serving['build_id']})"
    )
    return tools


def _base_state(
    info: dict,
    environment: dict[str, str],
    tools: dict,
    roots: tuple[Path, ...],
    project: Path,
) -> dict:
    observed_tools = {
        name: {
            "path": str(tool.path),
            "resolved_path": str(tool.path.resolve(strict=True)),
            "lookup_path": shutil.which(name, path=environment["PATH"])
            if name in {"bash", "uv"}
            else str(tool.path),
            "symlink_target": os.readlink(tool.path)
            if tool.path.is_symlink()
            else None,
            "sha256": _digest(tool.path),
            "version": tool.version,
        }
        for name, tool in tools.items()
    }
    # prog_size is derived from the stage ELF, not a compiler option. Everything
    # else in actual build-info remains immutable, including actual output path.
    configuration = {key: value for key, value in info.items() if key != "prog_size"}
    scripts = {
        str(path): _digest(path) for path in project.rglob("*.ld") if path.is_file()
    }
    result = {
        "tools": observed_tools,
        "sdk": {**_tree_hashes(roots), **scripts},
        "configuration": configuration,
        "environment": {
            key: hashlib.sha256(value.encode()).hexdigest()
            for key, value in environment.items()
        },
    }
    return result


def _state(
    info: dict,
    environment: dict[str, str],
    tools: dict,
    roots: tuple[Path, ...],
    project: Path,
) -> dict:
    result = _base_state(info, environment, tools, roots, project)
    if info.get("env") == "esp32dev":
        result["planned_configuration"] = xtensa_check.planned_configuration(info)
    if info.get("env") == "esp32c6":
        result["planned_configuration"] = mode_check.planned_configuration(info)
    return result


def _file_universe(root: Path) -> dict[str, str]:
    if not root.is_dir() or root.is_symlink():
        raise ValueError(f"unsafe staged source root {root}")
    result = {}
    for directory, dirs, files in os.walk(root, followlinks=False):
        base = Path(directory)
        if any((base / name).is_symlink() for name in dirs):
            raise ValueError("unsafe staged source directory symlink")
        for name in files:
            path = base / name
            if path.is_symlink() or not path.is_file():
                raise ValueError("unsafe staged source file")
            result[path.relative_to(root).as_posix()] = _digest(path)
    return result


def _check_staged_sources(
    workspace: Path, expected: dict[str, str], project: Path, producer: bytes = PRODUCER
) -> None:
    from ci.compiler.source_manager import (
        _GENERATED_EXAMPLE_DIRS,
        _GENERATED_EXAMPLE_FILES,
        generate_main_cpp,
    )

    # This profile has one ordinary local library, no unbound sibling libraries.
    if {path.name for path in (project / "lib").iterdir()} != {"FastLED"}:
        raise ValueError("unexpected staged project library universe")
    wanted = {
        path[4:]: digest for path, digest in expected.items() if path.startswith("src/")
    }
    metadata = workspace / "library.json"
    if not metadata.is_file() or metadata.is_symlink():
        raise ValueError("unsafe staged library metadata source")
    wanted["library.json"] = _digest(metadata)
    if _file_universe(project / "lib/FastLED") != wanted:
        raise ValueError("staged FastLED source universe differs")
    # Reuse the production source-manager's explicit generated-artifact policy.
    # Every copied source/asset/header is otherwise accounted for, not just .cpp.
    example = workspace / "examples/AutoResearch"
    sketch = _file_universe(example)
    wanted_project = {}
    inos = []
    for relative, digest in sketch.items():
        first = relative.split("/", 1)[0]
        if first in _GENERATED_EXAMPLE_DIRS or (
            "/" not in relative
            and (relative in _GENERATED_EXAMPLE_FILES or relative.endswith(".ino.cpp"))
        ):
            continue
        wanted_project["sketch/" + relative] = digest
        if "/" not in relative and relative.endswith(".ino"):
            inos.append(relative)
    if inos != ["AutoResearch.ino"]:
        raise ValueError("unexpected staged public producer .ino set")
    wanted_project["main.cpp"] = hashlib.sha256(
        generate_main_cpp(inos).encode()
    ).hexdigest()
    if _file_universe(project / "src") != wanted_project:
        raise ValueError("staged project source universe differs")
    if (project / "src/sketch/AutoResearch.ino").read_bytes() != producer:
        raise ValueError("staged fixed public producer changed")


def _section_bytes(elf: bytes) -> dict:
    # Closed ELF32 little-endian header extraction, not instruction decoding.
    if elf[:7] != b"\x7fELF\x01\x01\x01" or len(elf) < 52:
        raise ValueError("unsupported fixed RP2040 ELF format")
    if struct.unpack_from("<H", elf, 18)[0] != 40:
        raise ValueError("RP2040 ELF is not ARM")
    offset = struct.unpack_from("<I", elf, 32)[0]
    size, count, names = struct.unpack_from("<HHH", elf, 46)
    if size != 40 or names >= count or offset + size * count > len(elf):
        raise ValueError("malformed RP2040 ELF section table")
    sections = [
        struct.unpack_from("<IIIIIIIIII", elf, offset + i * size) for i in range(count)
    ]
    strings = sections[names]
    table = elf[strings[4] : strings[4] + strings[5]]
    result = {}
    for section in sections:
        name = table[section[0] :].split(b"\0", 1)[0].decode("ascii", errors="strict")
        if section[1] == 1 and section[2] & 2:
            if section[4] + section[5] > len(elf) or name in result:
                raise ValueError("invalid allocated ELF section storage")
            result[name] = {
                "vma": section[3],
                "bytes_hex": elf[section[4] : section[4] + section[5]].hex(),
            }
    return result


def _capture(
    workspace: Path, saved: Path, info: dict, environment: dict[str, str]
) -> dict:
    elf = saved / "firmware.elf"
    commands = {
        "text_disassembly": [info["objdump_path"], "-d", "-C", "-z", str(elf)],
        # .data is executable but GNU objdump -d omits it; inspect it explicitly.
        "ram_disassembly": [
            info["objdump_path"],
            "-D",
            "-C",
            "-z",
            "-j",
            ".data",
            str(elf),
        ],
        "elf_layout": [info["readelf_path"], "-W", "-S", "-l", "-s", str(elf)],
    }
    outputs = {}
    for name, command in commands.items():
        result = native._run(
            command,
            workspace,
            saved / (name + ".txt"),
            build=False,
            environment=environment,
        )
        if result.returncode != 0:
            raise ValueError(f"RP2040 SDK inspection failed: {name}")
        outputs[name] = result.stdout
    _json(saved / "inspection-commands.json", commands)
    headers = (
        "\n".join(
            re.findall(
                r"^[0-9a-f]+ <.+>:$", str(outputs["text_disassembly"]), re.MULTILINE
            )
        )
        + "\n"
    )
    # Preserve raw full outputs, but inspect only code-owned selected functions;
    # unrelated overloaded aliases do not expand the decoder universe.
    outputs["text_disassembly"] = _selected_disassembly(
        str(outputs["text_disassembly"])
    )
    outputs["ram_disassembly"] = _selected_disassembly(str(outputs["ram_disassembly"]))
    outputs["text_function_headers"] = headers
    outputs["memory_map"] = (saved / "firmware.map").read_text()
    outputs["section_bytes"] = _section_bytes(elf.read_bytes())
    _json(saved / "classifier-capture.json", outputs)
    return outputs


def _retain_artifacts(
    info: dict,
    saved: Path,
    project: Path,
    kind: _PropertyKind = _PropertyKind.PLACEMENT,
) -> dict[str, str]:
    release = Path(info["prog_path"]).parent
    sources = [
        release / name
        for name in ("firmware.elf", "firmware.bin", "compile_commands.raw.json")
    ]
    maps = [
        path
        for path in (release / "firmware.map", project / "firmware.map")
        if path.is_file()
    ]
    if _is_mode(kind):
        # Unlike RP placement, ESP mode assessors consume no ordinary map. A warm native
        # link-cache hit can restore only ELF; never borrow a bootstrap map.
        candidates = []
        for label, source in (
            ("release", release / "firmware.map"),
            ("project", project / "firmware.map"),
        ):
            if not source.exists() and not source.is_symlink():
                continue
            if source.is_symlink() or not source.is_file():
                raise ValueError("unsafe C6 diagnostic map candidate")
            destination = saved / "diagnostic-maps" / (label + "-firmware.map")
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
            sha256 = _digest(destination)
            if _digest(source) != sha256:
                raise ValueError("C6 diagnostic map changed during retention")
            candidates.append(
                {"source": str(source), "retained": str(destination), "sha256": sha256}
            )
        _json(
            saved / "map-diagnostics.json",
            {
                "status": "missing"
                if not candidates
                else "present"
                if len(candidates) == 1
                else "ambiguous",
                "consumed_by_property": False,
                "freshness": "unproven",
                "candidates": candidates,
            },
        )
    else:
        # Preserve the existing default/RP IRQ contract: the RP classifiers
        # consume the unique ordinary map and cannot proceed without it.
        if len(maps) != 1:
            raise ValueError("missing or ambiguous actual RP2040 link map")
        sources += maps
    for source in sources:
        if not source.is_file():
            profile = "ESP mode" if _is_mode(kind) else "RP2040"
            raise ValueError(f"missing actual {profile} artifact {source}")
        shutil.copy2(source, saved / source.name)
    for source in release.rglob("*.a"):
        destination = saved / "archives" / source.relative_to(release)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    return {str(path): _digest(path) for path in saved.rglob("*") if path.is_file()}


def _selected_disassembly(output: str) -> str:
    matches = list(re.finditer(r"^([0-9a-f]+) <(.+)>:\n", output, re.MULTILINE))
    selected = []
    seen = set()
    for index, match in enumerate(matches):
        name = match[2]
        if name not in property_check.PATTERNS and not (
            "my9221EmitRp" in name and ("veneer" in name or "unsigned short" in name)
        ):
            continue
        if name in seen:
            raise ValueError("ambiguous selected RP2040 function")
        seen.add(name)
        end = matches[index + 1].start() if index + 1 < len(matches) else len(output)
        selected.append(output[match.start() : end])
    return "\n".join(selected)


@dataclass(frozen=True)
class BuildSession:
    info: dict
    before: dict
    after: dict
    profile_after: dict


def _build_session(
    workspace: Path,
    saved: Path,
    environment: dict,
    tools: dict,
    roots: tuple[Path, ...],
    profile: dict,
    engine_profile: dict,
    info: dict,
    expected_inputs: dict,
    kind: _PropertyKind = _PropertyKind.PLACEMENT,
) -> BuildSession:
    project = workspace / ".build/fbuild" / _board(kind)
    # Complete all expensive static hashing before an instance-scoped window.
    before_static = _state(info, _environment(workspace), tools, roots, project)
    property_check._verify_profile_state(saved, "before", profile, before_static)
    _ensure_stage_engine(workspace, saved, environment, tools, engine_profile)
    before = _daemon_identity(workspace, saved, "build-before", environment, tools)
    _verify_engine_profile(saved, "build-before", engine_profile, before)
    updated = _compile_selected(workspace, saved, environment, kind)
    # No source/SDK/ELF hashing between submission completion and this capture.
    after = _daemon_identity(workspace, saved, "build-after", environment, tools)
    _verify_serving_engine(saved, "build-after", before, after)
    after_static = _state(updated, _environment(workspace), tools, roots, project)
    property_check._verify_profile_state(saved, "after-build", profile, after_static)
    native._verify_inputs(workspace, expected_inputs)
    if _is_mode(kind):
        _check_staged_sources(
            workspace,
            expected_inputs,
            project,
            _mode_helper(kind).producer_sources(workspace)[EXAMPLE],
        )
    else:
        _check_staged_sources(workspace, expected_inputs, project)
    return BuildSession(updated, before, after, after_static)


def _retain_controlled_source(
    project: Path, saved: Path, kind: _PropertyKind, source: bytes
) -> None:
    # SourceManager stages src/* directly into lib/FastLED/*, consistently
    # with the complete source-universe check above. Paths are closed specs.
    relative = _vendor_spec(kind).source.removeprefix("src/")
    staged = project / "lib/FastLED" / relative
    if not staged.is_file() or staged.is_symlink() or staged.read_bytes() != source:
        raise ValueError("controlled staged vendor source differs")
    (saved / "staged-controlled-source.h").write_bytes(staged.read_bytes())


def _one_stage(
    workspace: Path,
    artifacts: Path,
    name: str,
    source: bytes,
    initial: dict,
    profile: dict,
    tools: dict,
    roots: tuple[Path, ...],
    info: dict,
    environment: dict,
    engine_profile: dict,
    kind: _PropertyKind = _PropertyKind.PLACEMENT,
) -> tuple[VendorStage, dict]:
    spec = _vendor_spec(kind)
    saved = artifacts / name
    saved.mkdir(parents=True, exist_ok=True)
    project = workspace / ".build/fbuild" / _board(kind)
    (workspace / spec.source).write_bytes(source)
    expected = dict(initial)
    expected[spec.source] = hashlib.sha256(source).hexdigest()
    native._verify_inputs(workspace, expected)
    release = Path(info["prog_path"]).parent
    for filename in ("firmware.elf", "firmware.map", "firmware.bin"):
        old = release / filename
        if old.is_file():
            old.rename(saved / ("previous-" + filename))
    session = _build_session(
        workspace,
        saved,
        environment,
        tools,
        roots,
        profile,
        engine_profile,
        info,
        expected,
        kind,
    )
    updated = session.info
    hashes = (
        _retain_artifacts(updated, saved, project, kind)
        if _is_mode(kind)
        else _retain_artifacts(updated, saved, project)
    )
    capture = (
        _mode_helper(kind).capture_retained(workspace, saved, updated, environment)
        if _is_mode(kind)
        else _capture(workspace, saved, updated, environment)
    )
    assessment = _assess_property(capture, kind, name)
    errors = assessment.errors
    for relative in (spec.source, EXAMPLE):
        retained = saved / "production-sources" / relative
        retained.parent.mkdir(parents=True, exist_ok=True)
        retained.write_bytes((workspace / relative).read_bytes())
    _retain_controlled_source(project, saved, kind, source)
    after = native._verify_inputs(workspace, expected)
    # Local analysis can outlive an idle daemon. Its completed build window is
    # already bound; these are static checks, never a liveness exemption inside it.
    observed = _state(updated, _environment(workspace), tools, roots, project)
    property_check._verify_profile_state(saved, "after-inspection", profile, observed)
    if any(_digest(Path(path)) != digest for path, digest in hashes.items()):
        raise ValueError("retained RP2040 artifact changed during inspection")
    hashes.update(
        {str(path): _digest(path) for path in saved.rglob("*") if path.is_file()}
    )
    flags = _stage_property_flags(kind, assessment)
    result = VendorStage(
        name,
        0,
        expected,
        after,
        profile["tools"],
        observed["tools"],
        profile["configuration"],
        "planned-argv-not-complete-executed-receipt",
        hashes,
        errors,
        # Mode assessors bind conditional call sites, never root reachability.
        flags.public_producer_reachable,
        flags.callback_arguments_resolved,
        flags.masked_callee_literal_closure,
        session.before,
        session.after,
        assessment.irq_analysis,
        assessment.mode_analysis,
        assessment.xtensa_analysis,
    )
    _json(saved / "stage.json", asdict(result))
    return result, updated


def _run_vendor_proof(root: Path, kind: _PropertyKind) -> VendorProof:
    spec = _vendor_spec(kind)
    root = root.resolve()
    # Reject incomplete inputs before Git, provisioning, build or inspection.
    for name in (spec.source, EXAMPLE, "ci/boards.py", "compile"):
        if not (root / name).is_file() or (root / name).is_symlink():
            raise ValueError(f"missing or unsafe RP2040 proof input {name}")
    original_inputs = native._inputs(root)
    workspace, _ = native._snapshot(root, spec.snapshot_kind)
    artifacts = root / ".build" / spec.artifact_group / workspace.name
    # The wrapper honors explicit UV_PROJECT_ENVIRONMENT; create no local
    # alias or placeholder that would evade the complete snapshot universe.
    originals = {
        spec.source: (workspace / spec.source).read_bytes(),
        EXAMPLE: (workspace / EXAMPLE).read_bytes(),
    }
    producers = (
        _mode_helper(kind).producer_sources(workspace)
        if _is_mode(kind)
        else {EXAMPLE: PRODUCER}
    )
    for relative in producers:
        originals.setdefault(relative, (workspace / relative).read_bytes())
    profile = None
    tools = None
    info = None
    environment = _environment(workspace)
    records = []
    try:
        initial = dict(original_inputs)
        for relative, content in producers.items():
            (workspace / relative).write_bytes(content)
            initial[relative] = hashlib.sha256(content).hexdigest()
        native._verify_inputs(workspace, initial)
        bootstrap = artifacts / "bootstrap"
        client_tools = _client_tools(environment)
        desired_launcher = _observe_launcher(
            workspace, bootstrap, environment, client_tools
        )
        bootstrap_engine = _prepare_bootstrap_engine(
            workspace, bootstrap, environment, client_tools, desired_launcher
        )
        info = _compile_selected(workspace, bootstrap, environment, kind)
        bootstrap_after = _daemon_identity(
            workspace, bootstrap, "bootstrap-build-after", environment, client_tools
        )
        _verify_serving_engine(
            bootstrap, "bootstrap-build", bootstrap_engine, bootstrap_after
        )
        project = workspace / ".build/fbuild" / _board(kind)
        if _is_mode(kind):
            _check_staged_sources(workspace, initial, project, producers[EXAMPLE])
        else:
            _check_staged_sources(workspace, initial, project)
        roots = _profile_roots(info)
        tools = _tool_identities(info, workspace, environment, bootstrap)
        for key, identity in client_tools.items():
            if tools[key].path != identity.path or tools[key].sha256 != identity.sha256:
                raise ValueError("prebootstrap launcher tool identity changed")
        engine_profile = bootstrap_after
        _json(bootstrap / "engine-profile.json", _engine_semantics(engine_profile))
        profile = _state(info, environment, tools, roots, project)
        _json(bootstrap / "frozen-profile.json", profile)
        _json(
            bootstrap / "scope.json",
            {
                "bootstrap_only": True,
                "certified_stage": False,
                "complete_executed_argv": False,
            },
        )
        if _is_mode(kind):
            _retain_artifacts(info, bootstrap, project, kind)
        else:
            _retain_artifacts(info, bootstrap, project)
        native._verify_inputs(workspace, initial)
        stages = {
            "baseline": originals[spec.source],
            spec.mutant: (
                _mode_helper(kind).old_output_mode_mutant(originals[spec.source])
                if _is_mode(kind)
                else property_check.kernel_mutant(originals[spec.source])
                if kind is _PropertyKind.PLACEMENT
                else irq_check.irq_mutant(originals[spec.source])
            ),
            "restored": originals[spec.source],
        }
        if tuple(stages) != ("baseline", spec.mutant, "restored"):
            raise ValueError("invalid fixed vendor stage sequence")
        expected = dict(initial)
        for name, source in stages.items():
            native._verify_inputs(workspace, expected)
            stage, info = _one_stage(
                workspace,
                artifacts,
                name,
                source,
                initial,
                profile,
                tools,
                roots,
                info,
                environment,
                engine_profile,
                kind,
            )
            records.append(stage)
            expected = dict(stage.inputs_after)
        if (
            records[0].artifact_sha256s[str(artifacts / "baseline/firmware.elf")]
            == records[1].artifact_sha256s[
                str(artifacts / spec.mutant / "firmware.elf")
            ]
        ):
            raise ValueError("RP2040 XIP mutant ELF is not distinct")
        proof = VendorProof(
            spec.scope,
            "forced-GenericSoftwareSPIOutput-init-gpio2-3-public-PinMode-Output"
            if _is_mode(kind)
            else "direct-controller-init-setLeds-showLeds",
            False,
            False,
            workspace,
            records,
            bootstrap,
            _engine_semantics(engine_profile),
            "Bounded conditional-ELF Output-field property under explicitly recorded arrival, nonaliasing and normal-callee ABI/frame-memory assumptions; no BIN/runtime mapping, runtime GPIO/toggle, hardware execution, IRQ/cache/timing or complete executed argv. All full5067 and5066 groups unresolved."
            if _is_mode(kind)
            else "Bounded static potential public route/placement or saved-PRIMASK property only; no hardware execution, physical IRQ guarantee, termination, min/max timing, stack/frame runtime location or complete executed argv. All seven MY groups unresolved.",
        )
    except (
        OSError,
        ValueError,
        KeyError,
        TypeError,
        native.CalledProcessError,
        native.TimeoutExpired,
    ) as error:
        _json(
            artifacts / "failure.json",
            {"complete": False, "workspace": workspace, "error": str(error)},
        )
        label = "ESP output-mode" if _is_mode(kind) else "RP2040 ELF"
        raise ValueError(
            f"{label} proof failed; retained {workspace}: {error}"
        ) from error
    finally:
        pending = sys.exc_info()[1]
        errors = native._restore_sources(workspace, originals)
        try:
            native._verify_inputs(workspace, original_inputs)
            native._verify_inputs(root, original_inputs)
            if profile is not None:
                property_check._verify_profile_state(
                    artifacts,
                    "final-restored",
                    profile,
                    _state(
                        info,
                        _environment(workspace),
                        tools,
                        roots,
                        workspace / ".build/fbuild" / _board(kind),
                    ),
                )
        except (
            OSError,
            ValueError,
            KeyError,
            TypeError,
            native.CalledProcessError,
            native.TimeoutExpired,
        ) as error:
            errors.append(str(error))
        if errors:
            _json(
                artifacts / "failure.json",
                {
                    "complete": False,
                    "workspace": workspace,
                    "restoration_errors": errors,
                    "original_error": str(pending) if pending else None,
                },
            )
            raise ValueError(
                f"RP2040 restoration/profile verification failed; retained {workspace}: {errors}"
            ) from pending
    _json(artifacts / "proof.json", asdict(proof))
    return proof


# Read-only queries in a fresh child resolve the SAME FBUILD namespace/env as
# the actual single-example `fbuild project build` client. No ensure/start/stop.
DAEMON_IDENTITY_SCRIPT = r"""
import hashlib, json, os, pathlib, urllib.request
from fbuild import Daemon, __version__
import fbuild._native as client_extension
import sys
from ci.software_spi_my9221_rp2040_runner import _startup_environment_hashes
info = Daemon.status()
if info['version'] != __version__:
    raise ValueError('daemon/client package version mismatch')
if info['host'] != '127.0.0.1' or type(info['port']) is not int or not 0 < info['port'] < 65536:
    raise ValueError('unqualified daemon loopback endpoint')
origin = 'http://127.0.0.1:' + str(info['port'])
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
def get(path):
    with opener.open(origin + path, timeout=10) as response:
        return json.load(response)
health, image = get('/health'), get('/api/daemon/image-hash')
pid = info['pid']
if type(pid) is not int or pid <= 0 or health['pid'] != pid or image['pid'] != pid:
    raise ValueError('daemon API PID identity mismatch')
proc = pathlib.Path('/proc') / str(pid)
def ticks():
    return (proc / 'stat').read_text().rsplit(')', 1)[1].split()[19]
start = ticks()
exe = proc / 'exe'
loaded_sha = hashlib.sha256(exe.read_bytes()).hexdigest()
# This is the actual server startup environment, NOT the client's. Unknown
# keys are discarded before hashing; no complete environ or secret is emitted.
startup_environment = _startup_environment_hashes((proc / 'environ').read_bytes())
stat = exe.stat()
if ticks() != start:
    raise ValueError('daemon PID reused during image observation')
namespace = os.readlink(proc / 'ns/pid')
if namespace != os.readlink('/proc/self/ns/pid'):
    raise ValueError('daemon/client PID namespace mismatch')
if health['status'] != 'healthy' or info['status'] != 'running' or info['dev_mode'] is not True:
    raise ValueError('daemon health/cache mode mismatch')
if health['version'] != info['version'] or health['build_id'] != info['build_id']:
    raise ValueError('daemon API build identity mismatch')
print(json.dumps(dict(endpoint=origin, pid=pid, health_pid=health['pid'], image_pid=image['pid'],
    start_ticks=start, started_at=info['started_at'], cache_identity=info['cache_identity'],
    cache_dir=info['cache_dir'], cache_schema_version=info['cache_schema_version'],
    daemon_dir=info['daemon_dir'], dev_mode=info['dev_mode'], version=info['version'],
    build_id=info['build_id'], source_exe=health['source_exe'], source_mtime=health['source_mtime'],
    image_sha256=loaded_sha, image_blake3=image['blake3'], exe_target=os.readlink(exe),
    startup_environment=startup_environment,
    client_python=str(pathlib.Path(sys.executable).absolute()),
    client_extension_sha256=hashlib.sha256(pathlib.Path(client_extension.__file__).read_bytes()).hexdigest(),
    adjacent_daemon_sha256=hashlib.sha256((pathlib.Path(sys.executable).absolute().parent / 'fbuild-daemon').read_bytes()).hexdigest(),
    exe_device=stat.st_dev, exe_inode=stat.st_ino, pid_namespace=namespace)))
"""


def _daemon_identity(
    workspace: Path, saved: Path, phase: str, environment: dict[str, str], tools: dict
) -> dict:
    if sys.platform != "linux":
        raise ValueError(
            "RP2040 serving-engine observation currently requires Linux /proc"
        )
    result = native._run(
        [sys.executable, "-c", DAEMON_IDENTITY_SCRIPT],
        workspace,
        saved / f"serving-{phase}.log",
        build=False,
        environment=environment,
    )
    if result.returncode != 0:
        raise ValueError(f"serving daemon identity query failed; retained {saved}")
    identity = json.loads(result.stdout)
    _json(saved / f"serving-{phase}.json", identity)
    if identity["image_sha256"] != tools["fbuild-daemon"].sha256:
        raise ValueError(
            "actual serving daemon image differs from bound installed binary"
        )
    if (
        identity["client_python"] != str(tools["python"].path.absolute())
        or identity["client_extension_sha256"] != tools["fbuild-extension"].sha256
        or identity["adjacent_daemon_sha256"] != tools["fbuild-daemon"].sha256
    ):
        raise ValueError(
            "actual serving client Python/native/adjacent-daemon identity changed"
        )
    return identity


def _verify_serving_engine(
    saved: Path, phase: str, expected: dict, observed: dict
) -> None:
    changed = sorted(
        key
        for key in set(expected) | set(observed)
        if expected.get(key) != observed.get(key)
    )
    consistent = all(
        record.get("pid") == record.get("health_pid") == record.get("image_pid")
        for record in (expected, observed)
    )
    _json(
        saved / f"engine-{phase}.json",
        {
            "expected": expected,
            "observed": observed,
            "changed_fields": changed,
            "internally_consistent": consistent,
        },
    )
    if changed or not consistent:
        raise ValueError(
            f"serving daemon engine identity changed at {phase}; retained {saved}"
        )


# These fields identify one process lifetime, not the immutable provider/cache
# contract. No substring/prefix filtering: every unknown field remains compared.
ENGINE_INSTANCE_FIELDS = frozenset(
    (
        "pid",
        "health_pid",
        "image_pid",
        "start_ticks",
        "started_at",
        "exe_device",
        "exe_inode",
    )
)


def _engine_semantics(identity: dict) -> dict:
    return {
        key: value
        for key, value in identity.items()
        if key not in ENGINE_INSTANCE_FIELDS
    }


def _verify_engine_profile(
    saved: Path, phase: str, expected: dict, observed: dict
) -> None:
    before, after = _engine_semantics(expected), _engine_semantics(observed)
    changed = sorted(
        key for key in set(before) | set(after) if before.get(key) != after.get(key)
    )
    _json(
        saved / f"engine-profile-{phase}.json",
        {"expected": before, "observed": after, "changed_fields": changed},
    )
    if changed:
        raise ValueError(
            f"immutable serving engine profile identity changed at {phase}: {changed}"
        )


EXISTING_ENGINE_SCRIPT = r"""
import errno, json, re, socket
from fbuild import Daemon
try:
    info = Daemon.status()
except ConnectionError as error:
    # Distinguish refused loopback from an online but unqualified/hung service.
    # Native status resolves the exact client namespace URL; no PID-file guess.
    ports = re.findall(r'http://127\.0\.0\.1:(\d+)/api/daemon/info', str(error))
    if len(set(ports)) != 1:
        raise ValueError('cannot qualify existing engine endpoint') from error
    port = int(ports[0])
    if not 0 < port < 65536:
        raise ValueError('invalid client endpoint port')
    try:
        connection = socket.create_connection(('127.0.0.1',port),timeout=3)
    except OSError as connect_error:
        if connect_error.errno != errno.ECONNREFUSED:
            raise ValueError('engine absence not proved by connection refusal') from connect_error
        print(json.dumps(dict(absent=True,endpoint='http://127.0.0.1:'+str(port))))
    else:
        connection.close()
        raise ValueError('online engine failed read-only identity preflight')
else:
    if info['host'] != '127.0.0.1':
        raise ValueError('unqualified existing engine host')
    print(json.dumps(dict(absent=False,endpoint='http://127.0.0.1:'+str(info['port']))))
"""


NORMAL_ENSURE_SCRIPT = r"""
import hashlib, json, pathlib, sys
import fbuild._native as extension  # ty: ignore[unresolved-import]
from fbuild import Daemon
python = pathlib.Path(sys.executable).absolute()
image = pathlib.Path(extension.__file__).absolute()
daemon = python.parent / 'fbuild-daemon'
if str(python) != sys.argv[1] or hashlib.sha256(image.read_bytes()).hexdigest() != sys.argv[2]:
    raise ValueError('selected ensure client Python/native binding changed')
if str(pathlib.Path(sys.prefix).absolute()) != sys.argv[4] or python.parent != pathlib.Path(sys.prefix).absolute() / 'bin':
    raise ValueError('selected ensure Python is not the provisioned venv/bin client')
if hashlib.sha256(pathlib.Path('/proc/self/exe').read_bytes()).hexdigest() != sys.argv[5]:
    raise ValueError('selected ensure Python loaded image changed')
if not daemon.is_file() or hashlib.sha256(daemon.read_bytes()).hexdigest() != sys.argv[3]:
    raise ValueError('adjacent serving daemon missing/changed; PATH fallback prohibited')
if Daemon.ensure_running() is not True:
    raise ValueError('official normal daemon ensure failed')
print(json.dumps(dict(normal_ensure=True,python=str(python),native=str(image),adjacent_daemon=str(daemon))))
"""


def _launcher_script(ensure: bool) -> str:
    # Run in the same bound UV activation as compile's Python entrypoint.
    verification = NORMAL_ENSURE_SCRIPT.split("if Daemon.ensure_running()", 1)[0]
    preparation = r"""
import os
from ci.util.fbuild_runner import fbuild_child_env
from ci.software_spi_my9221_rp2040_runner import _startup_environment_hashes, STARTUP_ENVIRONMENT_KEYS
final_environment = fbuild_child_env()
reviewed = b'\0'.join(os.fsencode(key)+'='.encode()+os.fsencode(value) for key,value in final_environment.items() if key in STARTUP_ENVIRONMENT_KEYS)
observation = dict(startup_environment=_startup_environment_hashes(reviewed),
    client_python=str(python),client_prefix=str(pathlib.Path(sys.prefix).absolute()),
    client_extension_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
    adjacent_daemon_sha256=hashlib.sha256(daemon.read_bytes()).hexdigest())
"""
    action = ""
    if ensure:
        action = "os.environ.clear()\nos.environ.update(final_environment)\nif Daemon.ensure_running() is not True:\n    raise ValueError('official normal daemon ensure failed')\n"
    return verification + preparation + action + "print(json.dumps(observation))\n"


def _client_tools(environment: dict) -> dict[str, native.ToolIdentity]:
    import fbuild._native as extension  # ty: ignore[unresolved-import]

    python = Path(sys.executable).absolute()
    paths = {
        "python": python,
        "fbuild-extension": Path(extension.__file__).absolute(),
        "fbuild-daemon": python.parent / "fbuild-daemon",
    }
    selected = shutil.which("uv", path=environment["PATH"])
    if selected is None or python.parent != Path(sys.prefix).absolute() / "bin":
        raise ValueError("missing provisioned launcher identity")
    paths["uv"] = Path(selected)
    return {
        name: native.ToolIdentity(path, _digest(path), "prebootstrap client identity")
        for name, path in paths.items()
    }


def _launcher_command(tools: dict, ensure: bool) -> list[str]:
    python, extension, daemon, uv = (
        tools[key] for key in ("python", "fbuild-extension", "fbuild-daemon", "uv")
    )
    if daemon.path.absolute() != python.path.absolute().parent / "fbuild-daemon":
        raise ValueError("launcher adjacent daemon mismatch")
    if any(
        _digest(tool.path) != tool.sha256 for tool in (python, extension, daemon, uv)
    ):
        raise ValueError("launcher tool identity changed")
    return [
        str(uv.path),
        "run",
        "--no-sync",
        str(python.path),
        "-c",
        _launcher_script(ensure),
        str(python.path.absolute()),
        extension.sha256,
        daemon.sha256,
        str(Path(sys.prefix).absolute()),
        python.sha256,
    ]


def _observe_launcher(
    workspace: Path, saved: Path, environment: dict, tools: dict
) -> dict:
    result = native._run(
        _launcher_command(tools, False),
        workspace,
        saved / "launcher-observation.log",
        build=False,
        environment=environment,
    )
    if result.returncode != 0:
        raise ValueError("controlled launcher observation failed")
    observed = json.loads(result.stdout)
    if (
        observed.get("client_python") != str(tools["python"].path.absolute())
        or observed.get("client_prefix") != str(Path(sys.prefix).absolute())
        or observed.get("client_extension_sha256") != tools["fbuild-extension"].sha256
        or observed.get("adjacent_daemon_sha256") != tools["fbuild-daemon"].sha256
        or not isinstance(observed.get("startup_environment"), dict)
    ):
        raise ValueError("controlled launcher observation identity mismatch")
    _json(saved / "desired-launcher.json", observed)
    return observed


def _prepare_bootstrap_engine(
    workspace: Path, saved: Path, environment: dict, tools: dict, desired: dict
) -> dict:
    existing = _probe_existing_engine(workspace, saved, environment, tools, None)
    if existing is not None:
        if existing.get("startup_environment") != desired["startup_environment"]:
            raise ValueError(
                "foreign startup environment differs from controlled launcher"
            )
    else:
        _ensure_stage_engine(workspace, saved, environment, tools, {})
    actual = _daemon_identity(
        workspace, saved, "controlled-bootstrap", environment, tools
    )
    if actual.get("startup_environment") != desired["startup_environment"]:
        raise ValueError("actual startup environment differs from desired launcher")
    if existing is not None:
        _verify_serving_engine(saved, "bootstrap-reuse", existing, actual)
    return actual


def _probe_existing_engine(
    workspace: Path,
    saved: Path,
    environment: dict,
    tools: dict,
    expected_endpoint: str | None,
) -> dict | None:
    result = native._run(
        [sys.executable, "-c", EXISTING_ENGINE_SCRIPT],
        workspace,
        saved / "engine-preflight.log",
        build=False,
        environment=environment,
    )
    if result.returncode != 0:
        raise ValueError(
            f"unqualified online serving engine preflight; retained {saved}"
        )
    observation = json.loads(result.stdout)
    _json(saved / "engine-preflight.json", observation)
    if (
        expected_endpoint is not None
        and observation.get("endpoint") != expected_endpoint
    ):
        raise ValueError(
            "existing serving engine endpoint differs from frozen client namespace"
        )
    if observation.get("absent") is True:
        return None
    if observation.get("absent") is not False:
        raise ValueError("missing explicit serving engine absence/online observation")
    return _daemon_identity(workspace, saved, "preflight-online", environment, tools)


def _ensure_stage_engine(
    workspace: Path, saved: Path, environment: dict, tools: dict, expected: dict
) -> None:
    existing = _probe_existing_engine(
        workspace, saved, environment, tools, expected.get("endpoint")
    )
    if existing is not None:
        _verify_engine_profile(saved, "preflight-online", expected, existing)
    try:
        python = tools["python"]
        extension = tools["fbuild-extension"]
        daemon = tools["fbuild-daemon"]
    except KeyError as error:
        raise ValueError("missing serving engine client identity binding") from error
    if daemon.path.absolute() != python.path.absolute().parent / "fbuild-daemon":
        raise ValueError(
            "serving engine adjacent-daemon identity differs; PATH fallback prohibited"
        )
    uv = tools.get("uv")
    if uv is None or _digest(uv.path) != uv.sha256:
        raise ValueError("bound UV client identity unavailable or changed")
    # Match the actual wrapper's UV activation (not a normalized/dropped PATH).
    # Its provisioned interpreter and package/adjacent daemon are verified in
    # the child before official ensure may act on the namespace.
    command = [
        str(uv.path),
        "run",
        "--no-sync",
        str(python.path),
        "-c",
        _launcher_script(True),
        str(python.path.absolute()),
        extension.sha256,
        daemon.sha256,
        str(Path(sys.prefix).absolute()),
        python.sha256,
    ]
    result = native._run(
        command,
        workspace,
        saved / "normal-ensure.log",
        build=False,
        environment=environment,
    )
    if result.returncode != 0:
        raise ValueError(f"normal serving engine ensure failed; retained {saved}")


def run_rp2040_elf_proof(root: Path) -> VendorProof:
    return _run_vendor_proof(root, _PropertyKind.PLACEMENT)


def run_rp2040_irq_proof(root: Path) -> VendorProof:
    return _run_vendor_proof(root, _PropertyKind.IRQ)


def run_esp32_c6_mode_proof(root: Path) -> VendorProof:
    return _run_vendor_proof(root, _PropertyKind.ESP32_C6_MODE)


def run_esp32_xtensa_mode_proof(root: Path) -> VendorProof:
    return _run_vendor_proof(root, _PropertyKind.ESP32_XTENSA_MODE)
