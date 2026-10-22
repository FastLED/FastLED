"""Contract for the reproducible optional Arduino SPI hardware checks."""

import asyncio
from pathlib import Path

import pytest

from ci.autoresearch.args import Args


def test_spi_provider_autoresearch_args() -> None:
    args = Args.parse_args(
        [
            "esp32dev_idf44",
            "--spi-provider",
            "--spi-provider-native",
            "--upload-port",
            "/dev/ttyUSB0",
        ]
    )
    assert args.spi_provider
    assert args.spi_provider_native
    assert args.environment_positional == "esp32dev_idf44"


def test_spi_provider_staging_preserves_sources(tmp_path: Path) -> None:
    from ci.autoresearch.spi_provider import BINDING, inject_spi_provider_fixture

    sketch = tmp_path / "sketch"
    sketch.mkdir()
    rpc = sketch / "AutoResearchRemoteDriverMethods.cpp"
    rpc.write_text(BINDING + "\n}\n")
    fixture = tmp_path / "fixture.hpp"
    fixture.write_text('// fixture\n#include "SpiProviderFaults.hpp"\n')
    faults = tmp_path / "faults.hpp"
    faults.write_text("// shared SDK faults\n")
    inject_spi_provider_fixture(sketch, fixture, faults)
    assert 'remote.bind("spiProviderProbe"' in rpc.read_text()
    assert "spi_provider_probe::probe(args)" in rpc.read_text()
    assert (sketch / "SpiProviderHil.hpp").read_bytes() == fixture.read_bytes()
    assert (sketch / "SpiProviderFaults.hpp").read_bytes() == faults.read_bytes()
    assert fixture.read_text() == '// fixture\n#include "SpiProviderFaults.hpp"\n'
    with pytest.raises(ValueError, match="already instrumented"):
        inject_spi_provider_fixture(sketch, fixture, faults)


def test_spi_provider_dependencies_preserve_configuration(tmp_path: Path) -> None:
    from ci.autoresearch.spi_provider import configure_spi_provider_dependencies
    from ci.compiler.project_ini import ProjectIni

    ini = tmp_path / "platformio.ini"
    ini.write_text("[env:board]\nlib_deps = ExistingLib, FS\nboard = actual-board\n")
    configure_spi_provider_dependencies(ini, "board")
    configure_spi_provider_dependencies(ini, "board")
    project = ProjectIni.parseFile(ini)
    deps = project.get_option("env:board", "lib_deps") or ""
    assert deps.replace(",", "\n").split().count("SPI") == 1
    assert "ExistingLib" in deps and "FS" in deps
    assert project.get_option("env:board", "board") == "actual-board"


@pytest.mark.parametrize("native", [False, True])
def test_spi_provider_fault_defines(native: bool) -> None:
    from ci.autoresearch.spi_provider import spi_provider_defines

    defines = spi_provider_defines(native)
    assert "heap_caps_malloc=fl_test_heap_caps_malloc" in defines
    assert (
        "spi_device_get_trans_result=fl_test_spi_get_trans_result" in defines
    ) == native
    assert ("xQueueCreateMutex=fl_test_xQueueCreateMutex" in defines) != native


@pytest.mark.parametrize(
    "arguments",
    [
        ["--spi-provider"],
        ["esp32dev_idf44", "--spi-provider"],
        ["esp32s3", "--spi-provider", "--upload-port", "/dev/test"],
        [
            "esp32dev_idf44",
            "--spi-provider",
            "--embedded-fs",
            "--upload-port",
            "/dev/test",
        ],
        ["esp32dev_idf44", "--spi-provider", "--rmt", "--upload-port", "/dev/test"],
    ],
)
def test_spi_provider_rejects_unsafe_or_mixed_invocations(arguments: list[str]) -> None:
    from ci.autoresearch.spi_provider import run_spi_provider

    assert asyncio.run(run_spi_provider(Args.parse_args(arguments))) == 1


@pytest.mark.parametrize("native", [False, True])
@pytest.mark.parametrize(
    "failure", [None, "backend", "provider_calls", "heap", "missing_heap", "rpc"]
)
def test_spi_provider_deployment_and_rpc_contract(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, native: bool, failure: str | None
) -> None:
    from unittest.mock import AsyncMock, Mock

    from ci.autoresearch import spi_provider
    from ci.rpc_client import RpcResponse
    from ci.util.fbuild_runner import FbuildCommandResult

    project = tmp_path / "staged"
    stage = Mock(return_value=project)
    inject = Mock()
    dependencies = Mock()
    deploy = Mock(return_value=FbuildCommandResult(success=True, output=""))
    provider = not native
    data = {
        "success": True,
        "provider": provider,
        "providerCalls": 2,
        "heapStart": 100,
        "heapEnd": 100,
    }
    if failure == "backend":
        data["provider"] = not provider
    elif failure == "provider_calls":
        data["providerCalls"] = 0
    elif failure == "heap":
        data["heapEnd"] = 99
    elif failure == "missing_heap":
        del data["heapStart"]
        del data["heapEnd"]
    client = Mock()
    client.connect = AsyncMock()
    client.close = AsyncMock()
    client.send = AsyncMock(return_value=RpcResponse(failure != "rpc", data, ""))
    monkeypatch.setattr(spi_provider, "synthesise_autoresearch_project", stage)
    monkeypatch.setattr(spi_provider, "inject_spi_provider_fixture", inject)
    monkeypatch.setattr(
        spi_provider, "configure_spi_provider_dependencies", dependencies
    )
    monkeypatch.setattr(spi_provider, "run_fbuild_deploy", deploy)
    monkeypatch.setattr(spi_provider, "RpcClient", Mock(return_value=client))
    arguments = [
        "esp32dev_idf44",
        "--spi-provider",
        "--upload-port",
        "/dev/test",
        "--skip-lint",
    ]
    if native:
        arguments.append("--spi-provider-native")
    args = Args.parse_args(arguments)
    args.project_dir = tmp_path
    result = asyncio.run(spi_provider.run_spi_provider(args))
    expected_success = failure is None or (native and failure == "provider_calls")
    assert result == (0 if expected_success else 1)
    stage.assert_called_once_with(
        "esp32dev_idf44",
        tmp_path,
        args.verbose,
        extra_defines=spi_provider.spi_provider_defines(native),
    )
    fixture = (
        "tests/platforms/esp/32/drivers/spi/spi_hw_1_esp32.hpp"
        if native
        else "tests/platforms/arduino/spi/spi_arduino.hpp"
    )
    inject.assert_called_once_with(
        project / "src/sketch",
        tmp_path / fixture,
        tmp_path / "tests/platforms/esp/32/drivers/spi/spi_provider.hpp",
    )
    dependencies.assert_called_once_with(project / "platformio.ini", "esp32dev_idf44")
    deploy.assert_called_once_with(
        project,
        environment="esp32dev_idf44",
        upload_port="/dev/test",
        timeout=1800,
    )
    client.close.assert_awaited_once()
    assert all(
        call.args == ("spiProviderProbe",) for call in client.send.call_args_list
    )
    if expected_success:
        assert client.send.await_count == 5
