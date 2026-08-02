"""Board-definition contracts for Raspberry Pi Pico 2 and Pico 2 W."""

import re
from pathlib import Path

from ci.boards import create_board
from ci.compiler.sketch_filter import parse_filter_from_sketch, should_skip_sketch


REPO_ROOT = Path(__file__).resolve().parents[2]
AUTORESEARCH_INO = REPO_ROOT / "examples" / "AutoResearch" / "AutoResearch.ino"
AUTORESEARCH_PLATFORM = (
    REPO_ROOT / "examples" / "AutoResearch" / "AutoResearchPlatform.h"
)
MUTEX_DISPATCH = REPO_ROOT / "src" / "platforms" / "mutex.h"
MUTEX_IMPL = REPO_ROOT / "src" / "platforms" / "arm" / "rp" / "mutex_rp.cpp.hpp"
SEMAPHORE_DISPATCH = REPO_ROOT / "src" / "platforms" / "semaphore.h"
SEMAPHORE_IMPL = REPO_ROOT / "src" / "platforms" / "arm" / "rp" / "semaphore_rp.cpp.hpp"


def test_rp2350w_selects_the_pico_2_w_board_profile() -> None:
    board = create_board("rp2350w")

    assert board.board_name == "rp2350w"
    assert board.real_board_name == "rpipico2w"
    assert board.framework == "arduino"
    assert board.board_build_core == "earlephilhower"
    assert board.platform_packages is not None
    assert "arduino-pico/releases/download/4.5.3/rp2040-4.5.3.zip" in (
        board.platform_packages
    )

    ini = board.to_platformio_ini()
    assert "[env:rp2350w]" in ini
    assert "board = rpipico2w" in ini
    assert "board_build.core = earlephilhower" in ini


def test_rp2350_and_rp2350w_keep_distinct_board_profiles() -> None:
    pico_2 = create_board("rp2350")
    pico_2_w = create_board("rp2350w")

    assert pico_2.real_board_name == "rpipico2"
    assert pico_2_w.real_board_name == "rpipico2w"
    assert pico_2.get_real_board_name() != pico_2_w.get_real_board_name()


def test_autoresearch_filter_admits_both_rp2350_profiles() -> None:
    sketch_filter = parse_filter_from_sketch(AUTORESEARCH_INO)

    assert sketch_filter is not None
    for environment in ("rp2350", "rp2350w"):
        skip, reason = should_skip_sketch(create_board(environment), sketch_filter)
        assert skip is False, reason


def test_autoresearch_identity_prioritizes_pico_2_w() -> None:
    source = AUTORESEARCH_PLATFORM.read_text(encoding="utf-8")
    pico_2_w_branch = source.index("defined(ARDUINO_RASPBERRY_PI_PICO_2W)")
    generic_rp2350_branch = source.index("defined(FL_IS_RP2350)")

    assert pico_2_w_branch < generic_rp2350_branch
    assert 'return "Raspberry Pi Pico 2 W (RP2350)";' in source
    assert 'return "RP2350";' in source


def test_rp_platform_dispatches_real_mutex_and_semaphore_backends() -> None:
    for source_path in (
        MUTEX_DISPATCH,
        MUTEX_IMPL,
        SEMAPHORE_DISPATCH,
        SEMAPHORE_IMPL,
    ):
        source = source_path.read_text(encoding="utf-8")
        assert not re.search(r"\bFL_IS_RP2040\b", source), (
            f"{source_path} still guards on FL_IS_RP2040"
        )
        assert re.search(r"\bFL_IS_RP\b", source), (
            f"{source_path} does not guard on FL_IS_RP"
        )
