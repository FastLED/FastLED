"""Size ceilings must reject growth and missing measurements independently."""

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
