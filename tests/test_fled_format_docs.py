from pathlib import Path


PROJECT_ROOT = Path(__file__).parent.parent


def _read(path: str) -> str:
    return (PROJECT_ROOT / path).read_text(encoding="utf-8")


def test_fled_format_mirror_documents_v1_layout_and_authority() -> None:
    text = _read("src/fl/fled/FLED_FORMAT.md")

    assert "Authority:" in text
    assert "https://github.com/zackees/ledmapper/blob/main/docs/fled-format.md" in text
    assert (
        "https://github.com/zackees/ledmapper/blob/main/scripts/inspect-fled.mjs"
        in text
    )
    assert "| 0 | 4 | `magic`" in text
    assert "| 8 | 4 | `json_length`" in text
    assert "| `0x00` | `rgb8` | 3 |" in text
    assert "| `0x04` | `rgb565le` | 2 |" in text
    assert "| `0x05` | `rgb16_linear` | 6 |" in text
    assert "channels" in text
    assert "script.micropython" in text
    assert "script.wasm" in text


def test_fled_format_mirror_specifies_the_source_color_contract() -> None:
    """The `.fled` mirror must carry the full `video.color` contract.

    FastLED does not yet transform pixels by the declared source profile, but
    a file that fails to *specify* its color encoding is unrecoverable later:
    the numbers stop meaning anything definite. So the carry contract - the
    four independent fields, the default tuple, and the rejection rules - is
    spec'd and tested before any engine support exists.
    """
    text = _read("src/fl/fled/FLED_FORMAT.md")

    assert "## Source Color Metadata" in text

    # The four fields stay independent - never collapsed into one "BT.709".
    for field in ("`primaries`", "`transfer`", "`matrix`", "`range`"):
        assert field in text

    # The default tuple, stated exactly.
    assert '"primaries": "bt709"' in text
    assert '"transfer": "srgb"' in text
    assert '"matrix": "rgb"' in text
    assert '"range": "full"' in text

    # sRGB transfer is not the BT.709 OETF, and "none" is not a value.
    assert "not** the BT.709 camera OETF" in text
    assert '`"none"` is not a valid `transfer` value' in text

    # Per-format color classes, including the formats with no default tuple.
    assert "### Color classes by pixel format" in text
    assert "no defined tuple" in text

    # The rejection rules a validator must enforce.
    assert "### Validation rules" in text
    assert "never silently fall back" in text
    assert "reserved" in text

    # Forward compatibility: advisory for rgb8, mandatory via pixel_format.
    assert "### Forward compatibility" in text
    assert "advisory" in text
    assert "not a version bump" in text


def test_fled_readme_opens_with_format_spec_link() -> None:
    lines = _read("src/fl/fled/README.md").splitlines()

    assert lines[0] == "# fl::Fled"
    assert lines[1] == ""
    assert lines[2].startswith("> **On-disk format spec:** [FLED_FORMAT.md]")
    assert "./FLED_FORMAT.md" in lines[2]
    assert "zackees/ledmapper" in lines[2]


def test_video_and_agent_docs_route_to_local_fled_spec() -> None:
    video_entry = _read("src/fl/video/_build.cpp.hpp")
    agent_summary = _read("agents/docs/fled-format.md")
    claude = _read("CLAUDE.md")

    assert "../fled/FLED_FORMAT.md" in video_entry
    assert "src/fl/fled/FLED_FORMAT.md" in agent_summary
    assert "fl::Fled" in agent_summary
    assert "fl::Video" in agent_summary
    assert "MicroPython" in agent_summary
    assert "WASM" in agent_summary
    assert "agents/docs/fled-format.md" in claude
