from pathlib import Path

import pytest

from ci.compiler.source_manager import copy_example_source


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
    assert copy_example_source(project_root, build_dir, "Blink")

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
    assert copy_example_source(project_root, build_dir, "Blink")

    staged_sketch = build_dir / "src" / "sketch"
    assert (staged_sketch / "Blink.ino").is_file()
    assert not (staged_sketch / "compile_commands.json").exists()
