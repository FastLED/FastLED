"""Stage a synthesised ``.build/fbuild/<board>/`` project for ``bash autoresearch``.

This is the autoresearch-side counterpart of the staging logic ``bash compile``
already performs via ``ci/compiler/board_compiler.py::init_fbuild_project``.
Pulling autoresearch off root ``./platformio.ini`` (issue #3281) means
autoresearch must synthesise its own per-board ``platformio.ini`` (fbuild's
project-file format) from ``ci/boards.py`` before fbuild is invoked.

The output of :func:`synthesise_autoresearch_project` is a build directory at
``<project_root>/.build/fbuild/<board>/`` containing:

- ``platformio.ini`` — generated from the ``ci/boards.py`` entry exactly the
  same way ``bash compile`` does, with zero read-side dependency on root
  ``./platformio.ini``.
- ``src/sketch/`` — populated by copying ``examples/AutoResearch/`` so that
  autoresearch's existing sketch-resolver fallback (``build_dir / "src" /
  "sketch"``) sees the sketch where it expects.

Callers normally invoke this once per autoresearch run, after the board name
has been determined (positional, ``--env``, ``--lcd*`` default, or chip
auto-detect).
"""

from __future__ import annotations

from pathlib import Path

from ci.boards import create_board
from ci.compiler.path_manager import FastLEDPaths, resolve_project_root


def apply_ota_fixture_partitions(ini: "Path") -> None:
    """Point `board_build.partitions` at the OTA fixture table, exactly once.

    Replaces the generated key rather than appending a second one. Appending
    left two `board_build.partitions` entries in the same [env:esp32c6]
    section -- the framework's huge_app.csv and this one -- and the build
    tolerates that, so which table takes effect is decided by the ini parser
    rather than by us. "The fixture table was silently not applied" then
    looks identical from the outside to "it was applied". A fixture that may
    or may not be in effect is worse than either outcome. See FastLED#3956.

    Raises when there is no key to replace: a generated-layout change must
    fail loudly, not quietly build against the wrong partition table.
    """
    replacement = "board_build.partitions = src/sketch/esp32c6_ota_fixture.csv"
    lines = ini.read_text(encoding="utf-8").splitlines()

    matches: list[int] = []
    for index, line in enumerate(lines):
        if line.split("=", 1)[0].strip() == "board_build.partitions":
            matches.append(index)

    if not matches:
        raise RuntimeError(
            f"No board_build.partitions key to replace in {ini}; the generated "
            f"project layout has changed and the OTA fixture partition table "
            f"would not be applied"
        )

    # Every match, not the first. Replacing one and leaving another is the
    # same defect this function exists to fix, one layer down: the parser
    # would still be choosing between two keys, and the choice would still
    # not be ours. Applies whether the duplicate was already there or a
    # future generator adds one.
    for index in matches:
        lines[index] = replacement
    if len(matches) > 1:
        # Collapse them, so the file states the table once. Kept in reverse
        # so the earlier indices stay valid as they are removed.
        for index in reversed(matches[1:]):
            del lines[index]

    ini.write_text("\n".join(lines) + "\n", encoding="utf-8")


def synthesise_autoresearch_project(
    board_name: str,
    project_root: Path | None,
    verbose: bool,
    extra_defines: list[str] | None = None,
    ota_fixture: bool = False,
) -> Path:
    """Stage ``.build/fbuild/<board>/`` and write a synthesised ``platformio.ini``.

    Args:
        board_name: fbuild env name (e.g. ``"esp32c6"``, ``"esp32s3"``).
            Must resolve via ``ci.boards.create_board``.
        project_root: FastLED project root. ``None`` => resolve automatically.
        verbose: Forwarded to ``init_fbuild_project`` for diagnostic prints.
        extra_defines: Additional ``NAME=VALUE`` compile defines merged into
            the synthesised ``build_flags``. Used by driver-specific bench
            modes (e.g. ``--dma-spi`` adds ``FASTLED_LPC_SPI_DMA=1`` so the
            AutoResearchSpiDma handlers compile in — FastLED #3456).
        ota_fixture: Select the C6-only partition table that reserves space
            for a staged RP2350W firmware image during peer OTA validation.

    Returns:
        Absolute path to the staged build directory, ready to be handed to
        fbuild via ``run_fbuild_deploy(build_dir, environment=board_name, ...)``.

    Raises:
        RuntimeError: If board lookup fails or staging cannot complete.
    """
    # Local import keeps the autoresearch import graph slim until staging is
    # actually requested -- the compiler package is heavier than we want to
    # load just to print --help.
    from ci.compiler.board_compiler import init_fbuild_project

    root = (project_root or resolve_project_root()).resolve()
    board = create_board(board_name)

    paths = FastLEDPaths(board.board_name, project_root=root)
    build_dir = paths.build_dir

    # The staged project never reads root platformio.ini (#3278 / #3279).
    defines = ["FASTLED_OBJECTFLED_DIAGNOSTICS=1"]
    # AutoResearch's managed RP2350-family fixtures must remain recoverable
    # when the CDC endpoint is wedged or cannot be opened. Arduino-Pico exposes
    # the Pico SDK's target-bound USB reset interface behind this opt-in
    # define; keep it scoped to AutoResearch instead of changing normal RP
    # builds. Use the resolved board name so board aliases (rpipico2 and
    # rpipico2w) receive the same recovery interface as the canonical names.
    if board.board_name in {"rp2350", "rp2350w"}:
        defines.append("ENABLE_PICOTOOL_USB=1")
    if extra_defines:
        defines.extend(extra_defines)

    init_result = init_fbuild_project(
        board,
        verbose,
        "AutoResearch",
        paths,
        build_dir=build_dir,
        additional_defines=defines,
    )
    if not init_result.success:
        raise RuntimeError(
            f"Failed to synthesise autoresearch project for board "
            f"'{board_name}': {init_result.output}"
        )

    if ota_fixture:
        if board_name != "esp32c6":
            raise RuntimeError("The OTA fixture partition is only valid for esp32c6")
        partitions = build_dir / "src" / "sketch" / "esp32c6_ota_fixture.csv"
        if not partitions.is_file():
            raise RuntimeError(f"Missing OTA fixture partition table: {partitions}")
        ini = build_dir / "platformio.ini"
        apply_ota_fixture_partitions(ini)

    return build_dir
