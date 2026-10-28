"""Tests for the driver-enable-define lint (FastLED#4940 / #4949).

The check flags a preprocessor guard that turns platform/driver code on by
wrapping a `platforms/...` include in an `FL_*`/`FASTLED_*` macro. Guards the
tree already ships are amnestied; new ones must not pass.
"""

from pathlib import Path

from ci.lint_cpp.driver_enable_define_checker import (
    DriverEnableDefineChecker,
    _iter_source_files,
    load_amnesty,
)
from ci.util.check_files import FileContent
from ci.util.paths import PROJECT_ROOT


def _check(body: str, path: str = "src/platforms/x/y.h") -> list[str]:
    checker = DriverEnableDefineChecker()
    return checker.check_file_content(FileContent(path=path, content=body, lines=[]))


def test_new_opt_in_define_is_flagged() -> None:
    issues = _check(
        '#ifdef FL_MY_NEW_DRIVER\n#include "platforms/x/my_driver.h"\n#endif\n'
    )
    assert len(issues) == 1
    assert "FL_MY_NEW_DRIVER" in issues[0]


def test_retired_pattern_is_flagged() -> None:
    # The exact shape retired in #4944 must not be reintroduced.
    issues = _check(
        "#ifdef FL_ESP8266_UART\n"
        '#include "platforms/esp/8266/fastled_esp8266_uart.h"\n'
        "#endif\n"
    )
    assert len(issues) == 1


def test_amnestied_platform_selection_is_allowed() -> None:
    assert (
        _check('#if defined(FL_IS_ESP32)\n#include "platforms/esp/32/x.h"\n#endif\n')
        == []
    )


def test_include_guard_is_not_flagged() -> None:
    assert _check("#ifndef FL_FOO_H\n#define FL_FOO_H\nint x;\n#endif\n") == []


def test_scope_is_src_platforms_only() -> None:
    checker = DriverEnableDefineChecker()
    assert checker.should_process_file("/repo/src/platforms/a/b.h")
    assert not checker.should_process_file("src/fl/a.h")
    assert not checker.should_process_file("src/platforms/a/b.md")


def test_amnesty_is_populated() -> None:
    assert len(load_amnesty()) > 0


def test_shipped_tree_has_no_violations() -> None:
    """The committed amnesty must exactly cover the current tree.

    If this fails, either a new opt-in define was added (fix it: select the
    driver by template) or the tree moved and the amnesty needs regenerating
    with `uv run ci/lint_cpp/driver_enable_define_checker.py --generate-amnesty`.
    """
    checker = DriverEnableDefineChecker()
    offenders: list[str] = []
    for path in _iter_source_files(PROJECT_ROOT / "src" / "platforms"):
        content = Path(path).read_text(errors="ignore")
        offenders.extend(
            checker.check_file_content(
                FileContent(path=str(path), content=content, lines=[])
            )
        )
    assert offenders == []
