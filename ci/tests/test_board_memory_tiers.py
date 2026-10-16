"""Memory-tier board lists must name real boards (FastLED#4847).

A tier entry with no Board definition is silently dead: `bash compile <name>`
fails with "unsupported platform" and the tier never applies.
"""

from ci.boards import (
    ALL,
    HUGE_MEMORY_BOARDS,
    LOW_MEMORY_BOARDS,
    TINY_MEMORY_BOARDS,
    Board,
)


# Host pseudo-boards that are classified by name but are not fbuild targets.
HOST_PSEUDO_BOARDS = {"native"}


def test_every_memory_tier_name_is_a_registered_board() -> None:
    registered = {board.board_name.lower() for board in ALL}
    tiers = TINY_MEMORY_BOARDS | LOW_MEMORY_BOARDS | HUGE_MEMORY_BOARDS
    missing = {
        name for name in tiers if name.lower() not in registered
    } - HOST_PSEUDO_BOARDS
    assert not missing, f"memory-tier names with no Board definition: {sorted(missing)}"


def test_new_avr_and_renesas_boards_are_low_memory() -> None:
    by_name = {board.board_name: board for board in ALL}
    for name in ("nano", "uno", "mega"):
        assert by_name[name].memory_class == "low", name


def test_renesas_platform_fallback_is_low_memory() -> None:
    # uno_r4_minima is deliberately not in LOW_MEMORY_BOARDS: the renesas-ra
    # platform fallback is what classifies it.
    assert "uno_r4_minima" not in LOW_MEMORY_BOARDS
    board = Board(
        board_name="uno_r4_minima", platform="renesas-ra", add_board_to_all=False
    )
    assert board.memory_class == "low"
