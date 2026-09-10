"""The OTA fixture partition table must be applied unambiguously.

Calls the production function directly. An earlier draft of this file
re-implemented the replacement locally, which would have passed regardless
of what the shipped code does.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from ci.autoresearch.staging import apply_ota_fixture_partitions

# Measured on the bench, 2026-09-10. Both are what the flash log actually
# wrote, not estimates: the C6 image at 0x10000 and the RP2350W update image
# staged into spiffs. See FastLED#3956.
MEASURED_C6_APP_BYTES = 2_708_032
MEASURED_RP2350W_IMAGE_BYTES = 1_034_444

_FIXTURE = (
    Path(__file__).resolve().parents[2]
    / "examples"
    / "AutoResearch"
    / "esp32c6_ota_fixture.csv"
)


def _slots() -> dict[str, tuple[int, int]]:
    """Parse the fixture table into {name: (offset, size)}."""
    slots: dict[str, tuple[int, int]] = {}
    for line in _FIXTURE.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        fields = [field.strip() for field in stripped.split(",")]
        if len(fields) < 5:
            continue
        slots[fields[0]] = (int(fields[3], 0), int(fields[4], 0))
    return slots


def test_app_slot_clears_the_measured_image() -> None:
    """app0 must be larger than the image flashed into it.

    It was not: app0 held 0x290000 (2,686,976) while the flash wrote
    2,708,032 bytes at 0x10000, overrunning into spiffs by 20.6 KB. esptool
    does not police partition bounds, so this reported success and the board
    came up with no working application.
    """
    _, size = _slots()["app0"]
    assert size > MEASURED_C6_APP_BYTES, (
        f"app0 is {size:,} bytes but the measured image is "
        f"{MEASURED_C6_APP_BYTES:,}; it would overrun into the next partition"
    )


def test_spiffs_clears_the_rp2350w_image() -> None:
    """spiffs must hold the update image the fixture exists to carry."""
    _, size = _slots()["spiffs"]
    assert size > MEASURED_RP2350W_IMAGE_BYTES, (
        f"spiffs is {size:,} bytes but the RP2350W image is "
        f"{MEASURED_RP2350W_IMAGE_BYTES:,}"
    )


def test_partitions_do_not_overlap_or_exceed_the_flash() -> None:
    """A 4 MB part: the slots must tile without overlapping or overflowing."""
    flash_bytes = 0x400000
    ordered = sorted(_slots().items(), key=lambda item: item[1][0])
    previous_end = 0
    for name, (offset, size) in ordered:
        assert offset >= previous_end, f"{name} at 0x{offset:X} overlaps the slot before it"
        previous_end = offset + size
    assert previous_end <= flash_bytes, (
        f"partitions end at 0x{previous_end:X}, past the {flash_bytes:#x} flash"
    )


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
