from pathlib import Path

import pytest

from ci.boards import create_board
from ci.compiler.compiler import InitResult
from ci.compiler.path_manager import FastLEDPaths
from ci.compiler.pio import PioCompiler, init_fbuild_project


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
    paths = FastLEDPaths("esp8266", project_root=tmp_path)
    paths.home_dir = tmp_path / "home"
    paths.fastled_root = paths.home_dir / ".fastled"
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


@pytest.mark.parametrize(
    ("additional_defines", "expected_managed"),
    [([], ["FL_ESP8266_EMBEDDED_FS"]), (["FL_ESP8266_EMBEDDED_FS"], [])],
)
def test_compiler_seeds_owned_defines_from_first_sketch(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    additional_defines: list[str],
    expected_managed: list[str],
) -> None:
    compiler = object.__new__(PioCompiler)
    compiler.initialized = False
    compiler.board = create_board("esp8266")
    compiler.verbose = False
    compiler.paths = FastLEDPaths("esp8266", project_root=tmp_path)
    compiler.build_dir = tmp_path / "build"
    compiler.additional_defines = additional_defines
    compiler.additional_include_dirs = None
    compiler.additional_libs = None
    compiler.use_fbuild = True
    compiler._sketch_build_defines = []

    monkeypatch.setattr(
        "ci.compiler.pio._init_platformio_build",
        lambda *_args, **_kwargs: InitResult(
            success=True,
            output="",
            build_dir=compiler.build_dir,
            sketch_build_defines=["FL_ESP8266_EMBEDDED_FS"],
        ),
    )

    result = compiler._internal_init_build_no_lock("Assets")

    assert result.success
    assert compiler._sketch_build_defines == expected_managed
