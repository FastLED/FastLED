from pathlib import Path

import pytest

from ci.boards import create_board
from ci.compiler.path_manager import FastLEDPaths
from ci.compiler.pio import init_fbuild_project


@pytest.mark.parametrize(
    ("storage_declaration", "expects_define"),
    [("storage=littlefs\n", True), ("", False)],
)
def test_asset_requirement_reaches_generated_compile_configuration(
    tmp_path: Path, storage_declaration: str, expects_define: bool
) -> None:
    example_dir = tmp_path / "example"
    example_dir.mkdir()
    (example_dir / "Example.ino").write_text("void setup() {}\nvoid loop() {}\n")
    (example_dir / "data").mkdir()
    (example_dir / "data" / "asset.rgb.lnk").write_text(
        f"https://example.com/asset.rgb\n{storage_declaration}"
    )
    build_dir = tmp_path / "build"
    paths = FastLEDPaths("esp8266")
    paths._global_platformio_cache_dir = tmp_path / "pio-cache"

    result = init_fbuild_project(
        board=create_board("esp8266"),
        verbose=False,
        example=str(example_dir),
        paths=paths,
        build_dir=build_dir,
    )

    assert result.success
    generated_ini = (build_dir / "platformio.ini").read_text()
    assert ("-DFL_ESP8266_EMBEDDED_FS" in generated_ini) is expects_define
