"""Host C++ test tiers: tests/unit (default) and tests/integration (#5374).

The tier directory must never leak into a test name, and the integration tier
must stay out of every default run.
"""

import sys
from functools import cache
from pathlib import Path

from running_process import PIPE, RunningProcess

from ci.meson.test_execution import INTEGRATION_SUITE, meson_suite_args
from ci.util.smart_selector import strip_test_tier


ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tests"


@cache
def _organized() -> dict[str, tuple[str, str]]:
    """name -> (path, tier), as tests/meson.build receives them."""
    result = RunningProcess.run(
        [sys.executable, str(TESTS / "organize_tests.py"), str(TESTS)],
        stdout=PIPE,
        stderr=PIPE,
        encoding="utf-8",
        errors="replace",
        check=True,
        timeout=120,
    )
    rows = [
        line.split(":")
        for line in result.stdout.splitlines()
        if line.startswith("TEST:")
    ]
    names = [row[1] for row in rows]
    assert len(names) == len(set(names)), "test names must be unique across tiers"
    return {row[1]: (row[2], row[4]) for row in rows}


def test_names_ignore_the_tier() -> None:
    tests = _organized()
    assert tests["fl_channels_spi"] == ("unit/fl/channels/spi.cpp", "unit")
    assert tests["containers"] == ("unit/misc/containers.cpp", "unit")
    assert tests["noise"] == ("unit/noise.cpp", "unit")
    assert tests["colorimetric_strategy_table"] == (
        "integration/misc/colorimetric_strategy_table.cpp",
        "integration",
    )
    assert tests["mp3_simd_perf"] == (
        "integration/misc/mp3_simd_perf.cpp",
        "integration",
    )


def test_discovery_only_scans_tier_directories() -> None:
    tests = _organized()
    assert tests, "no tests discovered"
    for path, tier in tests.values():
        assert path.startswith(f"{tier}/"), path
    assert "colorimetric_strategy_checks" in tests
    # Shared infrastructure is not a test.
    assert "coroutine_impl" not in tests


def test_default_meson_test_excludes_integration() -> None:
    assert meson_suite_args(None, ["fastled:examples"], None) == [
        "--no-suite",
        "fastled:examples",
        "--no-suite",
        INTEGRATION_SUITE,
    ]
    # Already excluded under its bare name: not added twice.
    assert meson_suite_args(None, ["integration"], None) == [
        "--no-suite",
        "integration",
    ]


def test_integration_run_selects_only_its_suite() -> None:
    assert meson_suite_args(None, None, [INTEGRATION_SUITE]) == [
        "--suite",
        INTEGRATION_SUITE,
    ]


def test_named_test_is_not_filtered_by_tier() -> None:
    assert meson_suite_args("colorimetric_strategy_table", None, None) == []


def test_strip_test_tier() -> None:
    assert strip_test_tier("tests/unit/fl/async") == "fl/async"
    assert strip_test_tier("unit/fl/async") == "fl/async"
    assert strip_test_tier("tests/integration/misc/x.cpp") == "misc/x.cpp"
    assert strip_test_tier("tests/fl/async") == "fl/async"
    assert strip_test_tier("tests/profile/x.cpp") == "profile/x.cpp"


def test_test_case_outside_a_tier_fails_configuration(tmp_path: Path) -> None:
    (tmp_path / "unit").mkdir()
    (tmp_path / "unit" / "kept.cpp").write_text('FL_TEST_CASE("kept") {}\n')
    (tmp_path / "fl").mkdir()
    (tmp_path / "fl" / "stray.cpp").write_text('FL_TEST_CASE("stray") {}\n')
    result = RunningProcess.run(
        [sys.executable, str(TESTS / "organize_tests.py"), str(tmp_path)],
        stdout=PIPE,
        stderr=PIPE,
        encoding="utf-8",
        errors="replace",
        check=False,
        timeout=120,
    )
    assert result.returncode != 0
    assert "tests/fl/stray.cpp" in result.stderr
