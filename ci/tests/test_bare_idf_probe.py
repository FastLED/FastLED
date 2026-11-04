"""Regression coverage for bare-IDF flag stripping and the shrinking ratchet."""

from pathlib import Path

from ci.bare_idf_probe import arduino_include, bare_flags, compare_allowlist, entries


def test_strip_arduino_and_outputs() -> None:
    flags = bare_flags(
        [
            "g++",
            "-DARDUINO=10800",
            "-D",
            "ARDUINO_ARCH_ESP32",
            "-DESP32",
            "-I/sdk/cores/esp32",
            "-isystem",
            "/sdk/variants/esp32",
            "-I",
            "/sdk/libraries/WiFi/src",
            "-I/sdk/esp32/include",
            "-o",
            "main.o",
            "-MF",
            "main.d",
            "-MMD",
            "-c",
            "main.cpp",
        ],
        "main.cpp",
    )
    assert "-DESP32" in flags
    assert "-I/sdk/esp32/include" in flags
    assert "-DFL_NO_ARDUINO=1" in flags
    assert "-fsyntax-only" in flags
    assert not any(
        "ARDUINO_ARCH" in flag
        or "cores/" in flag
        or "variants/" in flag
        or "libraries/" in flag
        for flag in flags
    )
    assert not any(
        flag in flags for flag in ("-c", "-o", "main.cpp", "main.o", "main.d", "-MMD")
    )


def test_sdk_arduino_libs_are_idf_headers() -> None:
    assert not arduino_include("/sdk/tools/esp32-arduino-libs/esp32/include/driver")


def test_symlink_to_arduino_is_removed(tmp_path: Path) -> None:
    actual = tmp_path / "libraries/WiFi/src"
    actual.mkdir(parents=True)
    farm = tmp_path / "wifi-headers"
    farm.symlink_to(actual, target_is_directory=True)
    assert arduino_include(str(farm))


def test_compiledb_quoted_defines() -> None:
    flags = bare_flags(
        ["g++", '-DMBEDTLS_CONFIG_FILE=\\\\"mbedtls/esp_config.h\\\\"', "main.cpp"],
        "main.cpp",
    )
    assert '-DMBEDTLS_CONFIG_FILE="mbedtls/esp_config.h"' in flags


def test_allowlist_parser() -> None:
    assert entries("# header\n\nsrc/foo.cpp\nsrc/foo.cpp\n") == {"src/foo.cpp"}


def test_allowlist_cannot_grow() -> None:
    assert not compare_allowlist({"new.cpp"}, {"new.cpp"}, set())


def test_new_failure_rejected() -> None:
    assert not compare_allowlist({"new.cpp"}, set(), set())


def test_resolved_entry_must_be_removed() -> None:
    assert not compare_allowlist(set(), {"old.cpp"}, {"old.cpp"})
    assert compare_allowlist(set(), set(), {"old.cpp"})


def test_existing_failure_is_allowed() -> None:
    assert compare_allowlist({"old.cpp"}, {"old.cpp"}, {"old.cpp"})
