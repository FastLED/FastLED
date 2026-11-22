"""Keep controller storage stable across supported pre-include overrides (#5384)."""

import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize(
    "overrides",
    [
        "#define SKETCH_HAS_LARGE_MEMORY 0\n#define SKETCH_HAS_HUGE_MEMORY 0\n",
        "#define FL_PLATFORM_HAS_LARGE_MEMORY 0\n#define FL_PLATFORM_HAS_HUGE_MEMORY 0\n",
        "#define NO_CORRECTION 1\n",
    ],
    ids=["sketch-memory", "platform-memory", "no-correction"],
)
def test_controller_layout_matches_library(tmp_path: Path, overrides: str) -> None:
    compiler = shutil.which("clang++") or shutil.which("g++")
    if compiler is None:
        pytest.skip("host C++ compiler unavailable")

    objects: list[str] = []
    for name, definitions in [("library", ""), ("sketch", overrides)]:
        source = tmp_path / f"{name}.cpp"
        source.write_text(
            definitions
            + '#include "fl/channels/cled_controller.h"\n'
            + f'extern "C" unsigned {name}_size() {{ return sizeof(fl::CLEDController); }}\n',
            encoding="utf-8",
        )
        target = tmp_path / f"{name}.o"
        result = RunningProcess.run(
            [
                compiler,
                "-std=c++17",
                "-DFASTLED_STUB_IMPL=1",
                "-I",
                str(ROOT / "src"),
                "-c",
                str(source),
                "-o",
                str(target),
            ],
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            timeout=60,
        )
        assert result.returncode == 0, result.stdout
        objects.append(str(target))

    main = tmp_path / "main.cpp"
    main.write_text(
        '#include <cstdio>\nextern "C" unsigned library_size();\n'
        'extern "C" unsigned sketch_size();\n'
        'int main() { std::printf("library=%u sketch=%u\\n", library_size(), sketch_size());\n'
        "return library_size() == sketch_size() ? 0 : 1; }\n",
        encoding="utf-8",
    )
    binary = tmp_path / "layout"
    result = RunningProcess.run(
        [compiler, *objects, str(main), "-o", str(binary)],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=60,
    )
    assert result.returncode == 0, result.stdout
    result = RunningProcess.run(
        [str(binary)],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=10,
    )
    assert result.returncode == 0, result.stdout
