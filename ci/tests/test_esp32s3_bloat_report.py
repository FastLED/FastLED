"""The bloat gate must show where the flash went, not send you to rebuild it.

The gate holds `report.json` on the very runner that produced it, and used to
respond to a regression by telling the reader to run `bash bloat esp32s3`
locally -- which means an ESP32 toolchain and a three-minute compile to see
data CI already had (FastLED#4165).
"""

from __future__ import annotations

from typing import Any

from typeguard import typechecked

from tests.test_esp32s3_bloat_regression import largest_flash_symbols


@typechecked
def _symbol(
    size: int, name: str, region: str = "flash", archive: str = "libFastLED.a"
) -> dict[str, Any]:
    return {
        "mangled": f"_Z{len(name)}{name}",
        "demangled": name,
        "size": size,
        "region": region,
        "archive": archive,
        "object": "fl.gfx+_abcd.o",
    }


@typechecked
def test_largest_symbols_are_ordered_by_size() -> None:
    report = {
        "symbols": [
            _symbol(100, "small"),
            _symbol(9000, "huge"),
            _symbol(2500, "middling"),
        ]
    }
    got = largest_flash_symbols(report, 3)
    assert [s.name for s in got] == ["huge", "middling", "small"]
    assert [s.size for s in got] == [9000, 2500, 100]


@typechecked
def test_only_flash_symbols_are_counted() -> None:
    """A RAM symbol is not flash, and the gate is a flash gate."""

    report = {
        "symbols": [
            _symbol(9000, "in_ram", region="ram"),
            _symbol(120, "in_flash"),
        ]
    }
    got = largest_flash_symbols(report, 5)
    assert [s.name for s in got] == ["in_flash"]


@typechecked
def test_unsized_and_malformed_entries_are_skipped() -> None:
    """The report is data from another tool; it must not crash the gate.

    A regression report that raised here would replace a legible byte count
    with a traceback, which is worse than the behaviour being fixed.
    """

    report = {
        "symbols": [
            _symbol(0, "zero_sized"),
            {"demangled": "no_size", "region": "flash"},
            {"size": 500, "region": "flash"},  # no name at all
            "not a dict",
            _symbol(700, "real"),
        ]
    }
    got = largest_flash_symbols(report, 5)
    assert [s.name for s in got] == ["real", "(anonymous)"]


@typechecked
def test_a_report_without_symbols_yields_nothing() -> None:
    assert largest_flash_symbols({}, 10) == []
    assert largest_flash_symbols({"symbols": "not a list"}, 10) == []


@typechecked
def test_the_count_is_respected() -> None:
    report = {"symbols": [_symbol(i * 10, f"sym{i}") for i in range(1, 40)]}
    assert len(largest_flash_symbols(report, 25)) == 25
    assert len(largest_flash_symbols(report, 5)) == 5
