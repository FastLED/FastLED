"""Exercise the production Rust lint entry point against include boundaries."""

import json
from pathlib import Path

import pytest
from running_process import RunningProcess

from ci.lint_cpp.rust_binary_cache import ensure_rust_lint_binary


def _write(root: Path, name: str, source: str) -> Path:
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(source)
    return path


def _check(root: Path) -> list[dict[str, object]]:
    result = RunningProcess.run(
        [
            str(ensure_rust_lint_binary()),
            "--project-root",
            str(root),
            "--checker",
            "esp_arduino_header",
            "--format",
            "json",
        ],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=300,
    )
    assert result.returncode in (0, 1), result.stdout
    return json.loads(result.stdout)


@pytest.mark.parametrize("inactive", [False, True])
@pytest.mark.parametrize("sdk_header", ["FS.h", "vendor_wrapper.h"])
def test_native_wrapper_cannot_hide_arduino_dependency(
    tmp_path: Path, inactive: bool, sdk_header: str
) -> None:
    include = '#include "fl/detail/storage_wrapper.h"\n'
    _write(
        tmp_path,
        "src/platforms/esp/32/native.hpp",
        "#if 0\n" + include + "#endif\n" if inactive else include,
    )
    _write(tmp_path, "src/fl/detail/storage_wrapper.h", f"#include <{sdk_header}>\n")
    _write(tmp_path, f"sdk/{sdk_header}", "#include <Arduino.h>\n")
    _write(tmp_path, "sdk/Arduino.h", "#pragma once\n")
    unit = tmp_path / "src/platforms/esp/32/native.hpp"
    _write(
        tmp_path,
        "compile_commands.json",
        json.dumps(
            [
                {
                    "directory": str(tmp_path),
                    "file": str(unit),
                    "arguments": [
                        "c++",
                        "-DESP32=1",
                        "-I" + str(tmp_path / "src"),
                        "-I" + str(tmp_path / "sdk"),
                        "-c",
                        str(unit),
                    ],
                }
            ]
        ),
    )
    hits = _check(tmp_path)
    assert hits, "Native ESP -> repository wrapper -> FS.h -> Arduino.h escaped lint"
    diagnostic = "\n".join(str(hit["message"]) for hit in hits)
    assert "storage_wrapper.h" in diagnostic
    assert "Arduino.h" in diagnostic


def test_foreign_dispatch_branch_is_not_a_native_dependency(tmp_path: Path) -> None:
    _write(tmp_path, "src/platforms/esp/32/native.hpp", '#include "fl/dispatch.h"\n')
    _write(
        tmp_path,
        "src/fl/dispatch.h",
        "#if defined(FL_IS_AVR)\n#include <Arduino.h>\n#endif\n",
    )
    assert not _check(tmp_path)


@pytest.mark.parametrize(
    "directive",
    [
        "#include HIDDEN_HEADER",
        '#include HIDDEN("Arduino.h")',
        "#if __has_include(<Arduino.h>)\n#endif",
    ],
)
def test_wrapper_cannot_hide_unresolved_include_or_arduino_probe(
    tmp_path: Path, directive: str
) -> None:
    _write(tmp_path, "src/platforms/esp/32/native.hpp", '#include "fl/wrapper.h"\n')
    _write(tmp_path, "src/fl/wrapper.h", directive + "\n")
    assert _check(tmp_path), "Unresolved native closure must not silently pass"


@pytest.mark.parametrize(
    "header",
    ["platforms/arduino/application.hpp", "../platforms/arduino/application.hpp"],
)
def test_native_wrapper_cannot_enter_application_adapter(
    tmp_path: Path, header: str
) -> None:
    _write(tmp_path, "src/platforms/esp/32/native.hpp", '#include "fl/wrapper.h"\n')
    _write(tmp_path, "src/fl/wrapper.h", f'#include "{header}"\n')
    _write(
        tmp_path,
        "src/platforms/arduino/application.hpp",
        "// application-owned adapter\n",
    )
    assert _check(tmp_path)


def test_absolute_arduino_header_path_is_forbidden(tmp_path: Path) -> None:
    header = _write(tmp_path, "sdk/Arduino.h", "#pragma once\n")
    _write(tmp_path, "src/platforms/esp/32/native.hpp", f'#include "{header}"\n')
    assert _check(tmp_path)


def test_angle_include_uses_build_paths_not_includer_directory(tmp_path: Path) -> None:
    unit = _write(
        tmp_path, "src/platforms/esp/32/native.hpp", '#include "fl/wrapper.h"\n'
    )
    _write(tmp_path, "src/fl/wrapper.h", "#include <vendor.h>\n")
    _write(tmp_path, "src/fl/vendor.h", "#include <Arduino.h>\n")
    _write(tmp_path, "sdk/vendor.h", "#pragma once\n")
    _write(
        tmp_path,
        "compile_commands.json",
        json.dumps(
            [
                {
                    "directory": str(tmp_path),
                    "file": str(unit),
                    "arguments": [
                        "c++",
                        "-DESP32=1",
                        "-I" + str(tmp_path / "src"),
                        "-I" + str(tmp_path / "sdk"),
                        "-c",
                        str(unit),
                    ],
                }
            ]
        ),
    )
    assert not _check(tmp_path)


@pytest.mark.parametrize("first_host", [False, True])
def test_every_esp_compilation_entry_contributes_paths(
    tmp_path: Path, first_host: bool
) -> None:
    unit = _write(tmp_path, "src/platforms/esp/32/native.hpp", "#include <vendor.h>\n")
    _write(tmp_path, "sdk_good/vendor.h", "#pragma once\n")
    _write(tmp_path, "sdk_bad/vendor.h", "#include <Arduino.h>\n")
    entries = []
    for index, sdk in enumerate(["sdk_good", "sdk_bad"]):
        entries.append(
            {
                "directory": str(tmp_path),
                "file": str(unit),
                "arguments": [
                    "c++",
                    "-DHOST=1" if first_host and index == 0 else "-DESP32=1",
                    "-I" + str(tmp_path / "src"),
                    "-I" + str(tmp_path / sdk),
                    "-c",
                    str(unit),
                ],
            }
        )
    _write(tmp_path, "compile_commands.json", json.dumps(entries))
    assert _check(tmp_path), "Later native compilation entry was ignored"


def test_build_include_order_is_preserved(tmp_path: Path) -> None:
    unit = _write(tmp_path, "src/platforms/esp/32/native.hpp", "#include <shadow.h>\n")
    _write(tmp_path, "src/shadow.h", "#pragma once\n")
    _write(tmp_path, "sdk/shadow.h", "#include <Arduino.h>\n")
    _write(
        tmp_path,
        "compile_commands.json",
        json.dumps(
            [
                {
                    "directory": str(tmp_path),
                    "file": str(unit),
                    "arguments": [
                        "c++",
                        "-DESP32=1",
                        "-I" + str(tmp_path / "sdk"),
                        "-I" + str(tmp_path / "src"),
                        "-c",
                        str(unit),
                    ],
                }
            ]
        ),
    )
    assert _check(tmp_path), "Lint substituted a different header than the compiler"


@pytest.mark.parametrize(
    "directive",
    ["#define H <Arduino.h>\n#include H", "#if __has_include(<Arduino.h>)\n#endif"],
)
def test_repository_wrapper_outside_src_is_checked(
    tmp_path: Path, directive: str
) -> None:
    _write(
        tmp_path,
        "src/platforms/esp/32/native.hpp",
        '#include "../../../../include/wrapper.h"\n',
    )
    _write(tmp_path, "include/wrapper.h", directive + "\n")
    assert _check(tmp_path)


@pytest.mark.parametrize("guard", ["explicit", "feature", "none"])
def test_legacy_spi_escape_requires_its_explicit_dispatch_guard(
    tmp_path: Path, guard: str
) -> None:
    include = '#include "platforms/esp/32/core/fastspi_esp32_arduino.h"\n'
    condition = {
        "explicit": "defined(FL_ESP32_SPI_ARDUINO) && FL_ESP32_SPI_ARDUINO && defined(ARDUINO)",
        "feature": "UNRELATED_FEATURE",
    }.get(guard)
    source = f"#if {condition}\n{include}#endif\n" if condition else include
    _write(tmp_path, "src/platforms/esp/32/core/fastspi_esp32.h", source)
    _write(
        tmp_path,
        "src/platforms/esp/32/core/fastspi_esp32_arduino.h",
        '#pragma once\n#include "platforms/arduino/spi/fastspi_esp32_arduino.h"\n',
    )
    _write(
        tmp_path,
        "src/platforms/arduino/spi/fastspi_esp32_arduino.h",
        "#include <SPI.h>\n",
    )
    assert bool(_check(tmp_path)) is (guard != "explicit")


def test_legacy_compatibility_path_cannot_hide_implementation(tmp_path: Path) -> None:
    _write(
        tmp_path,
        "src/platforms/esp/32/core/fastspi_esp32_arduino.h",
        '#pragma once\n#include "platforms/arduino/spi/fastspi_esp32_arduino.h"\n#include <FS.h>\n',
    )
    assert _check(tmp_path)


def test_cycles_do_not_hide_a_forbidden_dependency(tmp_path: Path) -> None:
    _write(tmp_path, "src/platforms/esp/32/native.hpp", '#include "fl/a.h"\n')
    _write(tmp_path, "src/fl/a.h", '#include "fl/b.h"\n')
    _write(tmp_path, "src/fl/b.h", '#include "fl/a.h"\n#include <Arduino.h>\n')
    assert _check(tmp_path)


def test_comments_and_raw_strings_are_not_include_edges(tmp_path: Path) -> None:
    _write(tmp_path, "src/platforms/esp/32/native.hpp", '#include "fl/wrapper.h"\n')
    _write(
        tmp_path,
        "src/fl/wrapper.h",
        """// #include <Arduino.h>
/* #include <FS.h> */
const char* example = R"text(
#include <Arduino.h>
)text";
""",
    )
    assert not _check(tmp_path)


def test_native_c_hal_and_application_adapter_are_allowed(tmp_path: Path) -> None:
    _write(
        tmp_path, "src/platforms/esp/32/native.hpp", '#include "fl/detail/uart_hal.h"\n'
    )
    _write(tmp_path, "src/fl/detail/uart_hal.h", "#include <esp32-hal-uart.h>\n")
    _write(tmp_path, "src/platforms/arduino/application.hpp", "#include <Arduino.h>\n")
    assert not _check(tmp_path)


@pytest.mark.parametrize("good_flag,bad_flag", [("-isystem", "-I"), ("-I", "-iquote")])
def test_compiler_include_classes_take_precedence(
    tmp_path: Path, good_flag: str, bad_flag: str
) -> None:
    unit = _write(tmp_path, "src/platforms/esp/32/native.hpp", '#include "shadow.h"\n')
    _write(tmp_path, "sdk_good/shadow.h", "#pragma once\n")
    _write(tmp_path, "sdk_bad/shadow.h", "#include <Arduino.h>\n")
    _write(
        tmp_path,
        "compile_commands.json",
        json.dumps(
            [
                {
                    "directory": str(tmp_path),
                    "file": str(unit),
                    "arguments": [
                        "c++",
                        "-DESP32=1",
                        good_flag,
                        str(tmp_path / "sdk_good"),
                        bad_flag,
                        str(tmp_path / "sdk_bad"),
                        "-c",
                        str(unit),
                    ],
                }
            ]
        ),
    )
    assert _check(tmp_path), "Compiler include class precedence was ignored"
