"""The OTA fixture partition table must be applied unambiguously.

Calls the production function directly. An earlier draft of this file
re-implemented the replacement locally, which would have passed regardless
of what the shipped code does.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from ci.autoresearch.staging import apply_ota_fixture_partitions


def test_partition_key_is_replaced_not_duplicated(tmp_path: Path) -> None:
    """Exactly one key, and it names the fixture table.

    Appending left `huge_app.csv` and the fixture both present in
    [env:esp32c6], so which one took effect was up to the ini parser.
    """
    ini = tmp_path / "platformio.ini"
    ini.write_text(
        "[env:esp32c6]\n"
        "board = esp32c6\n"
        "board_build.partitions = huge_app.csv\n"
        "lib_archive = true\n",
        encoding="utf-8",
    )

    apply_ota_fixture_partitions(ini)

    text = ini.read_text(encoding="utf-8")
    keys = [ln for ln in text.splitlines() if ln.startswith("board_build.partitions")]
    assert len(keys) == 1, f"expected exactly one key, got {keys}"
    assert keys[0].endswith("esp32c6_ota_fixture.csv")
    assert "huge_app.csv" not in text
    assert "board = esp32c6" in text
    assert "lib_archive = true" in text


def test_missing_partition_key_fails_loudly(tmp_path: Path) -> None:
    """A layout change must not silently skip the fixture table.

    Appending always "worked"; replacing can find nothing to replace, and
    that has to be an error rather than a build that quietly uses the wrong
    partition layout.
    """
    ini = tmp_path / "platformio.ini"
    ini.write_text("[env:esp32c6]\nboard = esp32c6\n", encoding="utf-8")

    with pytest.raises(RuntimeError, match="board_build.partitions"):
        apply_ota_fixture_partitions(ini)
