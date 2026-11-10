"""Focused native linker (reld) selection and cache identity checks."""

import os
from pathlib import Path

import pytest
from running_process import PIPE, RunningProcess

from ci.meson import native_launchers
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


@pytest.fixture(autouse=True)
def _default_compiler_provider(monkeypatch: pytest.MonkeyPatch) -> None:
    """Keep default-linker tests independent of the invoking CI job's opt-in."""
    monkeypatch.delenv("FASTLED_NATIVE_CLANG", raising=False)


def _fake_bridge(tmp_path: Path, content: bytes = b"bridge") -> Path:
    bridge = tmp_path / "bridge-linker"
    bridge.write_bytes(content)
    return bridge


def _fake_reld(tmp_path: Path, name: str = "reld") -> Path:
    linker = tmp_path / name
    linker.write_bytes(b"one")
    linker.chmod(0o755)
    return linker


def test_native_linker_defaults_to_pinned_reld_and_exports_bridge(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    reld = _fake_reld(tmp_path)
    bridge = tmp_path / "bridge"
    bridge.write_bytes(b"b")
    monkeypatch.delenv("FASTLED_NATIVE_LINKER", raising=False)
    monkeypatch.delenv("RELD_BRIDGE_LINKER", raising=False)
    monkeypatch.setattr("ci.meson.meson_setup_execute.ensure_reld", lambda: reld)
    monkeypatch.setattr("ci.meson.meson_setup_execute.bridge_linker", lambda: bridge)
    selected, identity = resolve_native_linker("debug-thin")
    assert selected == str(reld)
    assert identity.startswith(f"{reld}:")
    assert os.environ["RELD_BRIDGE_LINKER"] == str(bridge)


@pytest.mark.parametrize("mode", ["quick", "debug", "debug-thin", "profile", "release"])
@pytest.mark.parametrize("host", ["linux", "darwin", "win32"])
def test_reld_links_every_mode_on_every_host(
    mode: str, host: str, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    # ThinLTO release included: reld routes LTO links to its bridge linker.
    monkeypatch.setattr("ci.meson.meson_setup_execute.sys.platform", host)
    monkeypatch.setattr(
        "ci.meson.meson_setup_execute.bridge_linker", lambda: _fake_bridge(tmp_path)
    )
    reld = _fake_reld(tmp_path)
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", str(reld))
    selected, identity = resolve_native_linker(mode)
    assert selected == str(reld)
    assert identity.startswith(f"{reld}:")


def test_native_linker_override_must_be_an_absolute_reld_executable(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(
        "ci.meson.meson_setup_execute.bridge_linker", lambda: _fake_bridge(tmp_path)
    )
    other = _fake_reld(tmp_path, "otherlinker")
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", str(other))
    with pytest.raises(ValueError, match="must be a reld executable"):
        resolve_native_linker("quick")
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", "reld")
    with pytest.raises(ValueError, match="absolute"):
        resolve_native_linker("quick")
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", str(tmp_path / "missing" / "reld"))
    with pytest.raises(ValueError, match="executable"):
        resolve_native_linker("quick")


def test_native_linker_content_change_requires_reconfigure(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    linker = _fake_reld(tmp_path)
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", str(linker))
    monkeypatch.setattr(
        "ci.meson.meson_setup_execute.bridge_linker", lambda: _fake_bridge(tmp_path)
    )
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
        native_linker="/opt/reld",
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
        native_linker="/opt/reld",
        native_linker_identity="/opt/reld:digest",
        enable_examples=False,
        enable_unit_tests=True,
        reconfigure=True,
    )
    assert "-Dnative_linker=/opt/reld" in command
    sidecar = _write_zccache_input_sidecar(
        build_dir=tmp_path / "build",
        build_mode="debug-thin",
        source_hashes=hashes,
        native_linker_identity="/opt/reld:digest",
    )
    assert sidecar is not None
    assert "native_linker=/opt/reld:digest" in sidecar.read_text()


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
        native_linker="/opt/reld",
        source_dir=tmp_path,
        env={},
    )
    assert calls == [
        ["meson", "setup", "--reconfigure", str(build_dir)],
        ["meson", "configure", str(build_dir), "-Dnative_linker=/opt/reld"],
    ]


def test_bridge_is_part_of_identity_and_overrides_stale_env(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    # CodeRabbit on #4632: a changed bridge must relink, and an inherited
    # RELD_BRIDGE_LINKER must never beat the pinned toolchain.
    reld = _fake_reld(tmp_path)
    bridge = _fake_bridge(tmp_path, b"one")
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", str(reld))
    monkeypatch.setenv("RELD_BRIDGE_LINKER", "/stale/bridge-linker")
    monkeypatch.setattr("ci.meson.meson_setup_execute.bridge_linker", lambda: bridge)
    _, first = resolve_native_linker("quick")
    assert os.environ["RELD_BRIDGE_LINKER"] == str(bridge)
    assert f"bridge={bridge}:" in first
    bridge.write_bytes(b"two")
    _, second = resolve_native_linker("quick")
    assert first != second


def test_missing_bridge_fails_loudly(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from ci.tools import reld as reld_tools

    monkeypatch.setattr(reld_tools.platform, "system", lambda: "Linux")
    import clang_tool_chain.wrapper as wrapper

    monkeypatch.setattr(wrapper, "find_tool_binary", lambda tool: tmp_path / "nope")
    with pytest.raises(FileNotFoundError, match="bridge linker"):
        reld_tools.bridge_linker()


def test_soldr_thinlto_bridge_stays_with_the_selected_compiler(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Release links use soldr's self-contained bridge behind the reld front end."""
    reld = _fake_reld(tmp_path)
    bridge = _fake_bridge(tmp_path, b"soldr bridge one")
    monkeypatch.setattr(native_launchers.sys, "platform", "linux")
    monkeypatch.setenv("FASTLED_NATIVE_CLANG", "soldr")
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", str(reld))
    monkeypatch.setenv("RELD_BRIDGE_LINKER", "/stale/clang-tool-chain/bridge")
    monkeypatch.setattr(
        "ci.meson.meson_setup_execute.bridge_linker",
        lambda: pytest.fail("soldr mode must not resolve clang-tool-chain's bridge"),
    )

    def soldr_print(flag: str) -> str:
        """Verify bridge lookup uses the supported soldr print interface."""
        assert flag == "--print-linker"
        return str(bridge)

    monkeypatch.setattr(native_launchers, "_soldr_print", soldr_print)
    selected, first = resolve_native_linker("release")
    assert selected == str(reld)
    assert os.environ["RELD_BRIDGE_LINKER"] == str(bridge)
    assert f"bridge={bridge}:" in first
    bridge.write_bytes(b"soldr bridge two")
    _, second = resolve_native_linker("release")
    assert first != second


def test_compiler_change_wipes_coredata_without_configure_cache(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Rediscover compilers instead of restoring a configure tree or reconfiguring."""
    monkeypatch.setattr(
        "ci.meson.meson_setup_execute.get_meson_executable", lambda: "/venv/meson"
    )
    monkeypatch.setattr(
        "ci.meson.meson_setup_execute._get_zccache_meson_configure_path",
        lambda: pytest.fail("compiler changes must bypass the configure cache"),
    )
    cmd = build_meson_setup_cmd(
        native_file_path=tmp_path / "meson_native.txt",
        build_dir=tmp_path,
        build_mode="release",
        native_linker="/opt/reld",
        native_linker_identity="reld identity",
        enable_examples=True,
        enable_unit_tests=True,
        enable_full_examples=True,
        reconfigure=False,
        wipe=True,
    )
    assert cmd[:4] == ["/venv/meson", "setup", "--wipe", str(tmp_path)]
    assert "--reconfigure" not in cmd
    assert cmd[cmd.index("--native-file") + 1] == str(tmp_path / "meson_native.txt")


def test_meson_wipe_preserves_updated_native_file(tmp_path: Path) -> None:
    """Use Meson's real wipe path to retain and reread a private native input."""
    from ci.meson.compiler import get_meson_executable

    source = tmp_path / "source"
    source.mkdir()
    (source / "meson.build").write_text(
        "project('native-provider-wipe')\nmessage(meson.get_external_property('provider'))\n",
        encoding="utf-8",
    )
    build = tmp_path / "build"
    native = build / "meson-private" / "fastled-native.ini"
    native.parent.mkdir(parents=True)
    native.write_text("[properties]\nprovider = 'first-provider'\n", encoding="utf-8")
    first = RunningProcess.run(
        [
            get_meson_executable(),
            "setup",
            str(build),
            str(source),
            "--native-file",
            str(native),
        ],
        stdout=PIPE,
        stderr=PIPE,
        encoding="utf-8",
        errors="replace",
        timeout=60,
        check=True,
    )
    assert "first-provider" in first.stdout
    native.write_text("[properties]\nprovider = 'second-provider'\n", encoding="utf-8")
    changed = RunningProcess.run(
        [
            get_meson_executable(),
            "setup",
            "--wipe",
            str(build),
            str(source),
            "--native-file",
            str(native),
        ],
        stdout=PIPE,
        stderr=PIPE,
        encoding="utf-8",
        errors="replace",
        timeout=60,
        check=True,
    )
    assert native.is_file()
    assert "second-provider" in changed.stdout
