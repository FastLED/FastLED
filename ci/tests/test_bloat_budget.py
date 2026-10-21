"""Size ceilings must reject growth and missing measurements independently."""

import sys

import pytest

from ci import bloat
from ci.bloat import verify_budget


def test_budget_rejects_flash_and_ram_growth() -> None:
    budget = {"image_flash": 100, "total_ram": 20}
    assert verify_budget({"image_flash": 100, "total_ram": 20}, budget) == []
    assert len(verify_budget({"image_flash": 101, "total_ram": 21}, budget)) == 2


def test_budget_does_not_substitute_attributed_flash_for_image_flash() -> None:
    failures = verify_budget(
        {"total_flash": 90, "total_ram": 20}, {"image_flash": 100, "total_ram": 20}
    )
    assert len(failures) == 1
    assert "image_flash" in failures[0]


def test_budget_rejects_missing_or_invalid_ram_ceiling() -> None:
    report = {"image_flash": 100, "total_ram": 20}
    for invalid in (None, -1, True, "20"):
        assert verify_budget(report, {"image_flash": 100, "total_ram": invalid})


def test_compare_rejects_single_profile_budget_before_building(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(
        sys,
        "argv",
        ["bloat", "esp32s3", "--compare", "--build", "--budget", "unused.json"],
    )

    def unexpected_build(*args: object) -> None:
        pytest.fail("Invalid budget/profile combination started a build")

    monkeypatch.setattr(bloat, "run_compile", unexpected_build)
    with pytest.raises(SystemExit, match="--compare cannot be used with --budget"):
        bloat.main()
