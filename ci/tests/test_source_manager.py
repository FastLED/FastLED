from pathlib import Path

import pytest

from ci.compiler.source_manager import CopyExampleResult, copy_example_source


@pytest.mark.parametrize("generated_dir", [".build", ".fbuild", ".pio", "fastled_js"])
def test_copy_example_source_skips_generated_directories(
    tmp_path: Path, generated_dir: str
) -> None:
    project_root = tmp_path / "project"
    example_dir = project_root / "examples" / "Blink"
    example_dir.mkdir(parents=True)
    (example_dir / "Blink.ino").write_text("void setup() {}\nvoid loop() {}\n")
    artifact = example_dir / generated_dir / "generated.cpp"
    artifact.parent.mkdir()
    artifact.write_text("#error generated artifact was staged\n")

    build_dir = tmp_path / "output"
    build_dir.mkdir()
    result = copy_example_source(project_root, build_dir, "Blink")
    assert result.success
    assert result.build_defines == []

    staged_sketch = build_dir / "src" / "sketch"
    assert (staged_sketch / "Blink.ino").is_file()
    assert not (staged_sketch / generated_dir).exists()


def test_copy_example_source_skips_generated_files(tmp_path: Path) -> None:
    project_root = tmp_path / "project"
    example_dir = project_root / "examples" / "Blink"
    example_dir.mkdir(parents=True)
    (example_dir / "Blink.ino").write_text("void setup() {}\nvoid loop() {}\n")
    (example_dir / "compile_commands.json").write_text("[]\n")

    build_dir = tmp_path / "output"
    build_dir.mkdir()
    assert copy_example_source(project_root, build_dir, "Blink").success

    staged_sketch = build_dir / "src" / "sketch"
    assert (staged_sketch / "Blink.ino").is_file()
    assert not (staged_sketch / "compile_commands.json").exists()


def test_copy_example_source_returns_embedded_fs_build_requirements(
    tmp_path: Path,
) -> None:
    project_root = tmp_path / "project"
    example_dir = project_root / "examples" / "Assets"
    example_dir.mkdir(parents=True)
    (example_dir / "Assets.ino").write_text("void setup() {}\nvoid loop() {}\n")
    (example_dir / "data").mkdir()
    (example_dir / "data" / "video.rgb.lnk").write_text(
        "https://example.com/video.rgb\nstorage=littlefs\n"
    )

    build_dir = tmp_path / "output"
    build_dir.mkdir()
    result = copy_example_source(project_root, build_dir, "Assets")

    assert isinstance(result, CopyExampleResult)
    assert result.success
    assert result.build_defines == ["FL_ESP8266_EMBEDDED_FS"]
