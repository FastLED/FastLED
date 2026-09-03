"""Unit tests for ci.meson.zccache_retry helpers (#4132)."""

from __future__ import annotations

from ci.meson.zccache_retry import (
    find_zccache_transient_failures,
    format_zccache_transient_message,
    is_zccache_transient_failure,
)


_OBJ_A = "ci/meson/native/fastled.so.p/.._.._.._src_fl_build_third_party+.cpp.o"
_OBJ_B = "ci/meson/native/fastled.so.p/.._.._.._src_fl_build_platforms+.cpp.o"

# Verbatim shape of the CI log cited in #4132 (job 100469403724).
_TRANSIENT_OUTPUT = (
    f"[16/618] Compiling C++ object {_OBJ_A}\n"
    f"\x1b[91mFAILED: [code=113] {_OBJ_A} \x1b[0m\n"
    '"/home/runner/work/FastLED/FastLED/.venv/bin/zccache" /home/runner/ctc-clang++ -I/x -c a.cpp\n'
    "zccache[info][Q]: daemon under load: queued, position 1 of 3 queued, 3 in flight\n"
    "zccache[info][Q]: daemon under load: compiling, position 0 of 3 queued, 3 in flight\n"
    "[clang-tool-chain] Added -shared-libasan for AddressSanitizer\n"
    f"[17/618] Compiling C++ object {_OBJ_B}\n"
    f"FAILED: [code=113] {_OBJ_B} \n"
    "src/fl/stl/not_null.cpp.hpp:24:41: warning: unused parameter 'message' [-Wunused-parameter]\n"
    "ninja: build stopped: subcommand failed.\n"
)


def test_finds_every_113_object_in_order() -> None:
    assert find_zccache_transient_failures(_TRANSIENT_OUTPUT) == [_OBJ_A, _OBJ_B]


def test_transient_when_no_diagnostic() -> None:
    """Warnings and zccache chatter do not count as a compiler diagnostic."""
    assert is_zccache_transient_failure(_TRANSIENT_OUTPUT)


def test_not_transient_with_real_compile_error() -> None:
    output = (
        _TRANSIENT_OUTPUT
        + "FAILED: [code=1] ci/meson/native/x.o\n"
        + "../../src/fl/foo.cpp:12:5: error: use of undeclared identifier 'bar'\n"
    )
    assert not is_zccache_transient_failure(output)


def test_not_transient_with_driver_error() -> None:
    output = (
        _TRANSIENT_OUTPUT + "clang: error: linker command failed with exit code 1\n"
    )
    assert not is_zccache_transient_failure(output)


def test_not_transient_with_ansi_coloured_error() -> None:
    """clang under -fdiagnostics-color=always: no word boundary before 'error'."""
    coloured = (
        "\x1b[1m../../src/fl/foo.cpp:12:5: \x1b[0m\x1b[0;1;31merror: \x1b[0m"
        "\x1b[1muse of undeclared identifier 'bar'\x1b[0m\n"
    )
    assert not is_zccache_transient_failure(_TRANSIENT_OUTPUT + coloured)


def test_not_transient_with_ansi_coloured_driver_error() -> None:
    coloured = (
        "clang++: \x1b[0;1;31merror: \x1b[0mlinker command failed with exit code 1\n"
    )
    assert not is_zccache_transient_failure(_TRANSIENT_OUTPUT + coloured)


def test_not_transient_with_fatal_error() -> None:
    output = _TRANSIENT_OUTPUT + "a.cpp:1:10: fatal error: 'nope.h' file not found\n"
    assert not is_zccache_transient_failure(output)


def test_not_transient_with_mixed_exit_codes() -> None:
    """A non-113 failure in the same run is not zccache, even with no diagnostic."""
    output = _TRANSIENT_OUTPUT + "FAILED: [code=1] ci/meson/native/x.o\n"
    assert not is_zccache_transient_failure(output)


def test_not_transient_without_113() -> None:
    output = "FAILED: [code=1] ci/meson/native/x.o\nninja: build stopped: subcommand failed.\n"
    assert not is_zccache_transient_failure(output)
    assert find_zccache_transient_failures(output) == []
    assert not is_zccache_transient_failure("")


def test_message_is_self_identifying() -> None:
    msg = format_zccache_transient_message([_OBJ_A])
    assert "113" in msg
    assert _OBJ_A in msg
    assert "no compilation was attempted" in msg
    assert "#4132" in msg
