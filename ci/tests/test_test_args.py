from ci.util.test_args import parse_args


def test_tiny_layout_query_bypasses_host_dll_smart_selection() -> None:
    parsed = parse_args(["--unit", "color_profile_tiny_layout"])
    assert parsed.test == "color_profile_tiny_layout"
    assert parsed.cpp
    assert parsed.unit
