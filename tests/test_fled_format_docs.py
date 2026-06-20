from pathlib import Path


PROJECT_ROOT = Path(__file__).parent.parent


def _read(path: str) -> str:
    return (PROJECT_ROOT / path).read_text(encoding="utf-8")


def test_fled_format_mirror_documents_v1_layout_and_authority() -> None:
    text = _read("src/fl/fled/FLED_FORMAT.md")

    assert "Authority:" in text
    assert "https://github.com/zackees/ledmapper/blob/main/docs/fled-format.md" in text
    assert "https://github.com/zackees/ledmapper/blob/main/scripts/inspect-fled.mjs" in text
    assert "| 0 | 4 | `magic`" in text
    assert "| 8 | 4 | `json_length`" in text
    assert "| `0x00` | `rgb8` | 3 |" in text
    assert "| `0x04` | `rgb565le` | 2 |" in text
    assert "channels" in text
    assert "script.micropython" in text
    assert "script.wasm" in text


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
