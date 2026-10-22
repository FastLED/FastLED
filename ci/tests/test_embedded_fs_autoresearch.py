"""Argument contract for the reproducible embedded-storage device checks."""

from pathlib import Path

import pytest

from ci.autoresearch.args import Args


def test_embedded_fs_autoresearch_args() -> None:
    args = Args.parse_args(
        [
            "esp32dev_idf44",
            "--embedded-fs",
            "--embedded-fs-provider",
            "--embedded-fs-format-fixture",
            "--upload-port",
            "/dev/ttyUSB0",
        ]
    )
    assert args.embedded_fs
    assert args.embedded_fs_provider
    assert args.embedded_fs_format_fixture
    assert args.environment_positional == "esp32dev_idf44"


def test_embedded_fs_staging_preserves_canonical_sources(tmp_path: Path) -> None:
    from ci.autoresearch.embedded_fs import BINDING, inject_embedded_fs_fixture

    sketch = tmp_path / "sketch"
    sketch.mkdir()
    rpc = sketch / "AutoResearchRemoteDriverMethods.cpp"
    rpc.write_text(BINDING + "\n}\n")
    fixture = tmp_path / "fixture.hpp"
    fixture.write_text("// hardware-only fixture\n")
    inject_embedded_fs_fixture(sketch, fixture)
    assert "embeddedFsSelfTest" in rpc.read_text()
    assert (sketch / "EmbeddedFsHil.hpp").read_bytes() == fixture.read_bytes()
    assert fixture.read_text() == "// hardware-only fixture\n"
    with pytest.raises(ValueError, match="already instrumented"):
        inject_embedded_fs_fixture(sketch, fixture)


def test_embedded_fs_dependencies_preserve_board_configuration(tmp_path: Path) -> None:
    from ci.autoresearch.embedded_fs import configure_embedded_fs_dependencies
    from ci.compiler.project_ini import ProjectIni

    ini = tmp_path / "platformio.ini"
    ini.write_text("[env:board]\nlib_deps = FS, LittleFS\nboard = actual-board\n")
    configure_embedded_fs_dependencies(ini, "board")
    configure_embedded_fs_dependencies(ini, "board")
    project = ProjectIni.parseFile(ini)
    assert project.get_option("env:board", "lib_deps") == "FS, LittleFS"
    assert project.get_option("env:board", "board") == "actual-board"


@pytest.mark.parametrize(
    "timeout,expected_seconds", [(None, 30.0), ("5s", 30.0), ("2m", 120.0)]
)
@pytest.mark.parametrize(
    "deployed_port,expected_port",
    [(None, "/dev/test"), ("/dev/application", "/dev/application")],
)
def test_embedded_fs_pipeline_timeout(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    timeout: str | None,
    expected_seconds: float,
    deployed_port: str | None,
    expected_port: str,
) -> None:
    import asyncio
    from unittest.mock import AsyncMock, Mock

    from ci.autoresearch import embedded_fs
    from ci.rpc_client import RpcResponse
    from ci.util.fbuild_runner import FbuildCommandResult

    project = tmp_path / "staged"
    stage = Mock(return_value=project)
    inject = Mock()
    dependencies = Mock()
    deploy = Mock(
        return_value=FbuildCommandResult(success=True, output="", port=deployed_port)
    )
    client = Mock()
    client.connect = AsyncMock()
    client.close = AsyncMock()
    client.send = AsyncMock(
        return_value=RpcResponse(
            True,
            {"success": True, "provider": False, "heapStart": 100, "heapEnd": 100},
            "",
        )
    )
    factory = Mock(return_value=client)
    monkeypatch.setattr(embedded_fs, "synthesise_autoresearch_project", stage)
    monkeypatch.setattr(embedded_fs, "inject_embedded_fs_fixture", inject)
    monkeypatch.setattr(embedded_fs, "configure_embedded_fs_dependencies", dependencies)
    monkeypatch.setattr(embedded_fs, "run_fbuild_deploy", deploy)
    monkeypatch.setattr(embedded_fs, "RpcClient", factory)
    arguments: list[str] = [
        "esp32dev_idf44",
        "--embedded-fs",
        "--upload-port",
        "/dev/test",
        "--skip-lint",
    ]
    if timeout is not None:
        arguments.extend(["--timeout", timeout])
    args = Args.parse_args(arguments)
    args.project_dir = tmp_path
    assert args.timeout == timeout

    assert asyncio.run(embedded_fs.run_embedded_fs(args)) == 0
    deploy.assert_called_once_with(
        project, environment="esp32dev_idf44", upload_port="/dev/test", timeout=1800
    )
    factory.assert_called_once_with(
        expected_port, timeout=expected_seconds, verbose=args.verbose
    )
    client.connect.assert_awaited_once()
    assert client.send.await_count == 2
    assert all(
        call.args == ("embeddedFsSelfTest",) for call in client.send.call_args_list
    )
    client.close.assert_awaited_once()
