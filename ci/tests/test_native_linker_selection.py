"""Focused native linker selection and cache identity checks."""

from pathlib import Path

import pytest

from ci.meson.meson_setup_execute import (
    _migrate_native_linker_option,
    _write_zccache_input_sidecar,
    build_meson_setup_cmd,
    invalidate_native_link_outputs,
    resolve_native_linker,
    run_meson_setup_command,
)
from ci.meson.meson_setup_phases import (
    CompilerDetection,
    MarkerPaths,
    SourceHashes,
    check_reconfigure_markers,
)


def test_native_linker_defaults_to_lld(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("FASTLED_NATIVE_LINKER", raising=False)
    assert resolve_native_linker("debug-thin") == ("lld", "lld")


@pytest.mark.parametrize("mode", ["quick", "debug", "debug-thin", "release", "profile"])
def test_native_linker_accepts_all_native_modes(
    mode: str, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr("ci.meson.meson_setup_execute.sys.platform", "linux")
    linker = tmp_path / "wild"
    linker.write_bytes(b"one")
    linker.chmod(0o755)
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", str(linker))
    selected, identity = resolve_native_linker(mode)
    assert selected == str(linker)
    assert identity.startswith(f"{linker}:")


def test_native_linker_requires_absolute_executable(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    linker = tmp_path / "wild"
    linker.write_bytes(b"one")
    linker.chmod(0o755)
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", str(linker))
    monkeypatch.setattr("ci.meson.meson_setup_execute.sys.platform", "linux")
    selected, identity = resolve_native_linker("debug-thin")
    assert selected == str(linker)
    assert identity.startswith(f"{linker}:")
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", "wild")
    with pytest.raises(ValueError, match="absolute"):
        resolve_native_linker("debug-thin")
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", str(tmp_path / "missing"))
    with pytest.raises(ValueError, match="executable"):
        resolve_native_linker("debug-thin")


def test_native_linker_override_is_linux_only(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    linker = tmp_path / "wild"
    linker.write_bytes(b"one")
    linker.chmod(0o755)
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", str(linker))
    for host in ("darwin", "win32"):
        monkeypatch.setattr("ci.meson.meson_setup_execute.sys.platform", host)
        with pytest.raises(ValueError, match="Linux-only"):
            resolve_native_linker("quick")


def test_native_linker_content_change_requires_reconfigure(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    linker = tmp_path / "wild"
    linker.write_bytes(b"one")
    linker.chmod(0o755)
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", str(linker))
    monkeypatch.setattr("ci.meson.meson_setup_execute.sys.platform", "linux")
    _, old_identity = resolve_native_linker("debug-thin")
    markers = MarkerPaths.for_build_dir(tmp_path)
    markers.native_linker.write_text(old_identity)
    linker.write_bytes(b"two")
    _, new_identity = resolve_native_linker("debug-thin")
    assert old_identity != new_identity
    hashes = SourceHashes("", "", "", [], 0.0)
    decision = check_reconfigure_markers(
        build_dir=tmp_path,
        markers=markers,
        hashes=hashes,
        debug=True,
        check=False,
        build_mode="debug-thin",
        native_linker_identity=new_identity,
        enable_examples=False,
        enable_full_examples=False,
        enable_unit_tests=True,
        use_thin_archives=True,
    )
    assert decision.force_reconfigure
    assert any("native linker" in reason for reason in decision.reasons)


def test_invalidation_removes_only_link_outputs(tmp_path: Path) -> None:
    (tmp_path / "tests").mkdir()
    linked = tmp_path / "tests" / "unit.so"
    linked.write_bytes(b"linked")
    obj = tmp_path / "tests" / "unit.o"
    obj.write_bytes(b"compiled")
    (tmp_path / "build.ninja").write_text(
        "build tests/unit.so: cpp_LINKER tests/unit.o\n"
        "build tests/unit.o: cpp_COMPILER source.cpp\n"
    )
    assert invalidate_native_link_outputs(tmp_path) == 1
    assert not linked.exists()
    assert obj.read_bytes() == b"compiled"


def test_link_outputs_invalidate_before_linker_marker_is_committed(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    (build_dir / "build.ninja").write_text(
        "build tests/unit.so: cpp_LINKER tests/unit.o\n"
    )
    (build_dir / "tests").mkdir()
    old_output = build_dir / "tests" / "unit.so"
    old_output.write_bytes(b"old linker output")
    markers = MarkerPaths.for_build_dir(build_dir)
    markers.native_linker.write_text("old linker identity")

    class Process:
        stdout = ""

        def __init__(self, command: list[str], **kwargs: object) -> None:
            pass

        def wait(self, **kwargs: object) -> int:
            return 0

    def commit_markers(**kwargs: object) -> None:
        assert not old_output.exists()
        markers.native_linker.write_text("new linker identity")

    monkeypatch.setattr("ci.meson.meson_setup_execute.RunningProcess", Process)
    monkeypatch.setattr(
        "ci.meson.meson_setup_execute._migrate_native_linker_option",
        lambda **kwargs: True,
    )
    monkeypatch.setattr(
        "ci.meson.meson_setup_execute._write_configuration_markers", commit_markers
    )
    monkeypatch.setattr(
        "ci.meson.meson_setup_execute.inject_ar_optimization_patches",
        lambda *args: None,
    )
    monkeypatch.setattr(
        "ci.meson.meson_setup_execute.normalize_meson_private_include_paths",
        lambda *args: False,
    )
    monkeypatch.setattr(
        "ci.meson.meson_setup_execute._enforce_strict_path_violations",
        lambda *args: None,
    )
    assert run_meson_setup_command(
        cmd=["meson", "setup"],
        source_dir=tmp_path,
        build_dir=build_dir,
        env={},
        markers=markers,
        hashes=SourceHashes("", "", "", [], 0.0),
        debug=True,
        check=False,
        build_mode="debug-thin",
        native_linker="/opt/wild",
        native_linker_identity="new linker identity",
        native_linker_changed=True,
        enable_examples=False,
        enable_full_examples=False,
        enable_unit_tests=True,
        use_thin_archives=True,
        compiler=CompilerDetection("", "", "", None, None, "", ""),
    )
    assert markers.native_linker.read_text() == "new linker identity"


def test_setup_command_and_cache_key_include_linker_identity(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(
        "ci.meson.meson_setup_execute.get_meson_executable", lambda: "meson"
    )
    monkeypatch.setattr(
        "ci.meson.meson_setup_execute._get_zccache_meson_configure_path",
        lambda: None,
    )
    hashes = SourceHashes("src", "test", "source", [], 0.0)
    command = build_meson_setup_cmd(
        native_file_path=tmp_path / "native.ini",
        build_dir=tmp_path / "build",
        build_mode="debug-thin",
        native_linker="/opt/wild",
        native_linker_identity="/opt/wild:digest",
        enable_examples=False,
        enable_unit_tests=True,
        reconfigure=True,
    )
    assert "-Dnative_linker=/opt/wild" in command
    sidecar = _write_zccache_input_sidecar(
        build_dir=tmp_path / "build",
        build_mode="debug-thin",
        source_hashes=hashes,
        native_linker_identity="/opt/wild:digest",
    )
    assert sidecar is not None
    assert "native_linker=/opt/wild:digest" in sidecar.read_text()


def test_old_build_dir_registers_native_linker_before_reconfigure(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    (build_dir / "build.ninja").write_text("")
    calls: list[list[str]] = []

    class Process:
        stdout = ""

        def __init__(self, command: list[str], **kwargs: object) -> None:
            calls.append(command)

        def wait(self, **kwargs: object) -> int:
            return 0

    monkeypatch.setattr("ci.meson.meson_setup_execute.RunningProcess", Process)
    monkeypatch.setattr(
        "ci.meson.meson_setup_execute.get_meson_executable", lambda: "meson"
    )
    marker = MarkerPaths.for_build_dir(build_dir).native_linker
    assert _migrate_native_linker_option(
        build_dir=build_dir,
        marker=marker,
        native_linker="/opt/wild",
        source_dir=tmp_path,
        env={},
    )
    assert calls == [
        ["meson", "setup", "--reconfigure", str(build_dir)],
        ["meson", "configure", str(build_dir), "-Dnative_linker=/opt/wild"],
    ]
