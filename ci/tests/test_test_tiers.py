"""Host C++ test tiers: tests/unit (default) and tests/integration (#5374).

The tier directory must never leak into a test name, and the integration tier
must stay out of every default run.
"""

import sys
from pathlib import Path

from ci.meson.test_execution import INTEGRATION_SUITE, meson_suite_args
from ci.util.smart_selector import strip_test_tier


ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tests"
sys.path.insert(0, str(TESTS))

from discover_tests import discover_test_files, split_tier  # noqa: E402
from test_config import EXCLUDED_TEST_DIRS, EXCLUDED_TEST_FILES  # noqa: E402
from test_helpers import extract_test_name  # noqa: E402


def test_names_ignore_the_tier() -> None:
    assert extract_test_name("unit/fl/channels/spi.cpp") == "fl_channels_spi"
    assert extract_test_name("unit/misc/containers.cpp") == "containers"
    assert extract_test_name("unit/noise.cpp") == "noise"
    assert (
        extract_test_name("integration/misc/colorimetric_strategy_table.cpp")
        == "colorimetric_strategy_table"
    )
    assert split_tier("integration/fl/x.cpp") == ("integration", "fl/x.cpp")


def test_discovery_only_scans_tier_directories() -> None:
    files = discover_test_files(TESTS, EXCLUDED_TEST_FILES, EXCLUDED_TEST_DIRS)
    assert files, "no tests discovered"
    assert all(f.startswith(("unit/", "integration/")) for f in files)
    assert "integration/misc/colorimetric_strategy_table.cpp" in files
    assert "unit/misc/colorimetric_strategy_checks.cpp" in files
    # Shared infrastructure is not a test.
    assert "unit/misc/coroutine_impl.cpp" not in files


def test_every_discovered_name_is_unique_across_tiers() -> None:
    files = discover_test_files(TESTS, EXCLUDED_TEST_FILES, EXCLUDED_TEST_DIRS)
    names = [extract_test_name(f) for f in files]
    assert len(names) == len(set(names))


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
