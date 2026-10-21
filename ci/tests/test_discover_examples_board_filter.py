from ci.meson import discover_examples_all as d


def test_wildcard_board_filter_does_not_exclude_host_builds() -> None:
    # `(board is not atmega8*)` must not drop the example from STUB builds
    # (#4805: the `\w+` value pattern missed the wildcard, dropping RGBW,
    # RGBWEmulated and AnalogOutput from host example tests).
    skip, _ = d.should_skip_for_stub("(board is not atmega8*)")
    assert skip is False


def test_platform_filter_still_excludes_host_builds() -> None:
    skip, _ = d.should_skip_for_stub("(platform is esp32)")
    assert skip is True
