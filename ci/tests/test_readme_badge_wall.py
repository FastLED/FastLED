"""Regression contracts for the README per-board build badge wall."""

import re
from pathlib import Path

from ci.boards import ALL, create_board


REPO_ROOT = Path(__file__).resolve().parents[2]
README = REPO_ROOT / "README.md"
WORKFLOWS = REPO_ROOT / ".github" / "workflows"

NON_BOARD_BUILD_WORKFLOWS = {
    "build_clone_and_compile.yml",
    "build_template.yml",
    "build_template_binary_size.yml",
    "build_template_custom_board.yml",
}

# These profiles intentionally reuse another workflow, are aliases/fallbacks,
# or are exercised outside the per-board badge wall.
KNOWN_UNBADGED_ALL_BOARD_ALIASES = {
    "digispark-tiny",
    "esp32dev_heapdbg",
    "esp32dev_qemu",
    "esp32rmt_51",
    "nice_nano_nrf52840",
    "nrfmicro_nrf52840",
    "nucleo_g070rb",
    "rp2350w",
    "seeed_xiao_esp32s3",
    "web",
    "xiaoblesense_adafrui",
}

AUDITED_SAMD_BADGES = {
    "build_samd21.yml": ("samd21_adafruit_feather_m0", "samd21"),
    "build_samd21_zero.yml": ("samd21_zeroUSB", "samd21_zero"),
    "build_samd51j.yml": ("samd51j_adafruit_feather_m4", "samd51j"),
    "build_samd51p.yml": ("samd51p_adafruit_grand_central_m4", "samd51p"),
    "build_metro_m4.yml": ("samd51j_adafruit_metro_m4", "metro_m4"),
}


def _readme_build_badges() -> dict[str, str]:
    readme = README.read_text(encoding="utf-8")
    matches = re.findall(
        r"\[!\[([^]]+)]\(https://github\.com/FastLED/FastLED/actions/"
        r"workflows/(build_[^/)]+\.yml)/badge\.svg\)]",
        readme,
    )
    return {workflow: label for label, workflow in matches}


def test_every_board_build_workflow_has_a_readme_badge_and_back() -> None:
    badges = _readme_build_badges()
    workflows = {path.name for path in WORKFLOWS.glob("build_*.yml")}
    board_workflows = workflows - NON_BOARD_BUILD_WORKFLOWS

    assert set(badges) - {"build_clone_and_compile.yml"} == board_workflows


def test_all_board_profiles_are_badged_or_explicitly_excluded() -> None:
    workflow_aliases: set[str] = set()
    for workflow_path in WORKFLOWS.glob("build_*.yml"):
        workflow = workflow_path.read_text(encoding="utf-8")
        workflow_aliases.update(re.findall(r"^\s*args:\s+([^\s]+)", workflow, re.M))

    all_board_aliases = {board.board_name for board in ALL if board.add_board_to_all}
    assert all_board_aliases - workflow_aliases == KNOWN_UNBADGED_ALL_BOARD_ALIASES


def test_samd_badges_name_the_mcu_and_actual_board() -> None:
    badges = _readme_build_badges()

    for workflow_name, (expected_label, board_alias) in AUDITED_SAMD_BADGES.items():
        assert badges[workflow_name] == expected_label

        board = create_board(board_alias)
        workflow = (WORKFLOWS / workflow_name).read_text(encoding="utf-8")
        assert re.search(rf"\bargs:\s+{re.escape(board_alias)}(?:\s|$)", workflow)
        assert board.real_board_name is not None
        assert board.real_board_name in expected_label


def test_samd51p_workflow_pins_the_viable_linux_toolchain_host() -> None:
    workflow = (WORKFLOWS / "build_samd51p.yml").read_text(encoding="utf-8")

    assert "runs-on: ubuntu-latest" in workflow


def test_build_badges_do_not_use_dangling_star_footnotes() -> None:
    readme = README.read_text(encoding="utf-8")

    assert not re.search(
        r"\]\(https://github\.com/FastLED/FastLED/actions/workflows/"
        r"build_[^/)]+\.yml\)\*",
        readme,
    )
