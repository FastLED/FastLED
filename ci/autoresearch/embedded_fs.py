"""Stage the ESP32 storage fixture into canonical AutoResearch and verify RPC."""

import asyncio
import json
import sys
from pathlib import Path

from running_process import RunningProcess

from ci.autoresearch.args import Args
from ci.autoresearch.staging import synthesise_autoresearch_project
from ci.boards import create_board
from ci.compiler.project_ini import ProjectIni
from ci.rpc_client import RpcClient
from ci.util.fbuild_runner import run_fbuild_deploy
from ci.util.sketch_resolver import parse_timeout


BINDING = "void AutoResearchRemoteControl::bindDriverMethods(fl::Remote& remote) {"


def inject_embedded_fs_fixture(sketch: Path, fixture: Path) -> None:
    """Inject a hardware-only test without modifying or adding repo examples."""
    rpc = sketch / "AutoResearchRemoteDriverMethods.cpp"
    text = rpc.read_text()
    if text.count(BINDING) != 1 or "embeddedFsSelfTest" in text:
        raise ValueError(
            "AutoResearch driver binding changed or is already instrumented"
        )
    (sketch / "EmbeddedFsHil.hpp").write_bytes(fixture.read_bytes())
    text = '#include "EmbeddedFsHil.hpp"\n' + text
    text = text.replace(
        BINDING,
        BINDING + '\n    remote.bind("embeddedFsSelfTest", '
        "[](const fl::json& args) -> fl::json { return embedded_fs_hil::probe(args); });",
        1,
    )
    rpc.write_text(text)


def configure_embedded_fs_dependencies(ini: Path, environment: str) -> None:
    """Preserve board dependencies while adding bundled filesystem headers."""
    project = ProjectIni.parseFile(ini)
    section = f"env:{environment}"
    existing = project.get_option(section, "lib_deps", "") or ""
    names = {item.strip() for item in existing.replace(",", "\n").splitlines()}
    additions = [name for name in ("FS", "LittleFS") if name not in names]
    project.set_option(section, "lib_deps", "\n".join([existing, *additions]).strip())
    project.dump(ini)


async def run_embedded_fs(args: Args) -> int:
    """Build/deploy and run both cold and warm filesystem lifecycle checks."""
    environment = args.environment_positional or args.environment
    if not environment or not args.upload_port:
        print(
            "--embedded-fs requires an explicit ESP32 environment and --upload-port",
            file=sys.stderr,
        )
        return 1
    if create_board(environment).platform_family != "esp32":
        print("The embedded-storage device fixture requires ESP32", file=sys.stderr)
        return 1
    separate_modes = (
        args.parlio,
        args.rmt,
        args.spi,
        args.uart,
        args.lcd,
        args.lcd_spi,
        args.lcd_rgb,
        args.object_fled,
        args.flex_io,
        args.all,
        args.simd,
        args.coroutine,
        args.ieee754,
        args.math,
        args.fft,
        args.mp3,
        args.net,
        args.net_server,
        args.net_client,
        args.net_peer,
        args.ota,
        args.ble,
        args.decode,
        args.watchdog_soak,
        args.rpc_smoke,
        args.perf_wave2d,
    )
    if any(separate_modes):
        print(
            "Run embedded-storage checks separately from LED/network/special modes",
            file=sys.stderr,
        )
        return 1
    root = args.project_dir.resolve()
    if not args.skip_lint:
        lint = await asyncio.to_thread(
            RunningProcess.run,
            ["bash", "lint"],
            cwd=str(root),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=900,
        )
        print(lint.stdout)
        if lint.returncode:
            return lint.returncode
    project = synthesise_autoresearch_project(
        environment,
        root,
        args.verbose,
        extra_defines=[
            f"FL_TEST_EMBEDDED_FS_PROVIDER={int(args.embedded_fs_provider)}",
            f"FL_TEST_EMBEDDED_FS_FORMAT_FIXTURE={int(args.embedded_fs_format_fixture)}",
        ],
    )
    inject_embedded_fs_fixture(
        project / "src/sketch",
        root / "tests/platforms/esp/fs/embedded_fs_esp.hpp",
    )
    # These bundled libraries provide headers to the explicit Arduino provider;
    # the native path uses only the SDK C component, and drops their archives.
    ini = project / "platformio.ini"
    configure_embedded_fs_dependencies(ini, environment)
    deployed = await asyncio.to_thread(
        run_fbuild_deploy,
        project,
        environment=environment,
        upload_port=args.upload_port,
        timeout=1800,
    )
    if not deployed.success:
        print(deployed.output)
        return 1
    client = RpcClient(
        args.upload_port,
        timeout=max(
            30.0, parse_timeout(args.timeout) if args.timeout is not None else 30.0
        ),
        verbose=args.verbose,
    )
    await client.connect()
    try:
        for phase in ("cold", "warm"):
            result = await client.send("embeddedFsSelfTest", args={})
            print(
                f"embedded_fs {phase}: {json.dumps(result.data, sort_keys=True)}",
                flush=True,
            )
            if not result.success or not result.get("success"):
                return 1
            if result.get("provider") != args.embedded_fs_provider:
                print("Filesystem fixture used the wrong backend", file=sys.stderr)
                return 1
            if args.embedded_fs_provider and not result.get("providerCalls", 0):
                print("Arduino filesystem provider was not called", file=sys.stderr)
                return 1
            if phase == "warm" and result.get("heapEnd") != result.get("heapStart"):
                print("Warm filesystem cycles retained heap", file=sys.stderr)
                return 1
    finally:
        await client.close()
    print("Embedded-storage AutoResearch passed (cold + warm)")
    return 0
