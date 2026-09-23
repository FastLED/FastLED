"""The shared native CI pilot must remain manual until coverage is proven."""

from pathlib import Path

from ci.util.test_args import parse_args
from ci.util.test_types import determine_test_categories, process_test_flags


ROOT = Path(__file__).resolve().parents[2]


def test_cpp_preset_includes_unit_and_examples() -> None:
    args = process_test_flags(parse_args(["--cpp", "--build-mode", "debug-thin"]))
    categories = determine_test_categories(args)
    assert categories.unit and categories.examples
    assert not categories.py and not categories.wasm


def test_native_pilot_is_manual_and_uses_shared_cpp_path() -> None:
    workflow = (ROOT / ".github/workflows/fractional_native_pilot.yml").read_text()
    assert "  workflow_dispatch:" in workflow
    assert "  push:" not in workflow
    assert "  pull_request:" not in workflow
    assert "bash test --cpp --build-mode debug-thin" in workflow
