"""reld is the only host linker: every other linker selection fails lint."""

from pathlib import Path

import pytest

from ci.lint.banned_linkers import (
    ALLOWED_PATHS,
    ALLOWED_TOKENS,
    find_banned_linkers,
    is_in_scope,
    run_banned_linkers_lint,
    scan,
)


@pytest.mark.parametrize(
    "line",
    [
        "core_link_args = ['-fuse-ld=lld']",
        "cpp_flags = ['-fuse-ld=mold']",
        "link_args += ['--ld-path=/usr/bin/ld.gold']",
        "sudo apt-get install -y lld llvm",
        "ld.lld --version",
        "LINKER = 'lld-link'",
        "tool = 'ld64.lld'",
        "RUSTFLAGS=-Clinker=rust-lld",
        "curl -L https://example/wild-linker-0.10.0.tar.gz",
        "use ld.bfd here",
        # CodeRabbit on #4632: toolchain launchers embed the linker name.
        "clang-tool-chain-ld obj.o -o app",
        "ctc-ld.lld --version",
        "ctc-ld64.lld -arch arm64",
    ],
)
def test_other_linkers_are_flagged(line: str) -> None:
    assert find_banned_linkers(line + "\n"), line


@pytest.mark.parametrize(
    "line",
    [
        'fl::printf("%lld", value)',
        "long long v; // printed with %lld",
        "native_linker = ensure_reld()",
        "uses: ./.github/workflows/template_unit_test.yml",
        "# wildcard patterns are fine",
        "llvm-ar --version",
        "self.lldb = find_debugger()",
    ],
)
def test_non_linker_text_is_not_flagged(line: str) -> None:
    assert find_banned_linkers(line + "\n") == []


def test_scope_is_the_host_build_surface() -> None:
    assert is_in_scope("ci/meson/compile.py")
    assert is_in_scope(".github/workflows/build_linux.yml")
    assert is_in_scope("meson.build")
    assert is_in_scope("tests/meson.build")
    assert is_in_scope("meson.options")
    # Board (fbuild) and WebAssembly links live under src/ and stay out of scope.
    assert not is_in_scope("src/platforms/wasm/compiler/build_flags.toml")
    assert not is_in_scope("src/platforms/wasm/meson.build")
    assert not is_in_scope("ci/docs/linkers.md")
    assert not is_in_scope("tests/fl/stl/stdio.cpp")


def test_allowlist_stays_small_and_names_the_reld_wiring() -> None:
    # Growth here means another linker is creeping back; each entry needs a reason.
    assert len(ALLOWED_PATHS) <= 12
    # Only the enforcement code may skip a whole file.
    assert {p for p, t in ALLOWED_TOKENS.items() if t is None} == {
        "ci/lint/banned_linkers.py",
        "ci/tests/test_banned_linkers.py",
    }
    assert "ci/meson/native/meson.build" in ALLOWED_PATHS
    assert "ci/tools/reld.py" in ALLOWED_PATHS


def test_repository_uses_only_reld() -> None:
    assert scan() == []


def test_run_fails_on_violation(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    (tmp_path / "ci").mkdir()
    (tmp_path / "ci" / "build.py").write_text("LINK = ['-fuse-ld=mold']\n")
    assert run_banned_linkers_lint(tmp_path) is False
    assert "ci/build.py:1: '-fuse-ld=mold'" in capsys.readouterr().out


def test_run_passes_on_clean_tree(tmp_path: Path) -> None:
    (tmp_path / "ci").mkdir()
    (tmp_path / "ci" / "build.py").write_text("LINK = [ensure_reld()]\n")
    assert run_banned_linkers_lint(tmp_path) is True


def test_allowed_file_still_rejects_other_linkers(tmp_path: Path) -> None:
    # CodeRabbit on #4632: allowlisting the reld wiring file must not let a
    # second linker selection slip in next to it.
    wiring = tmp_path / "ci" / "meson" / "native"
    wiring.mkdir(parents=True)
    (wiring / "meson.build").write_text(
        "core_link_args = ['-fuse-ld=lld', '--ld-path=' + native_linker]\n"
        "extra = ['-fuse-ld=mold']\n"
    )
    violations = scan(tmp_path)
    assert [(v.line_no, v.token) for v in violations] == [(2, "-fuse-ld=mold")]


def test_every_match_on_a_line_is_checked() -> None:
    hits = find_banned_linkers(
        "args = ['-fuse-ld=lld', '--ld-path=/x/reld', 'mold']\n",
        frozenset({"-fuse-ld=lld", "--ld-path="}),
    )
    assert [token for _, token, _ in hits] == ["mold"]
