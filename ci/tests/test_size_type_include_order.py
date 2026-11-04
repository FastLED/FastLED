"""Compile size types in every order, including a 32-bit embedded ABI."""

import itertools
import platform
import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize(
    "headers",
    list(
        itertools.permutations(
            [
                "stddef.h",
                "fl/stl/cstddef.h",
                "fl/stl/stdint.h",
            ]
        )
    ),
)
@pytest.mark.parametrize("abi", [[], ["-m32"]])
def test_size_type_include_order(
    tmp_path: Path, headers: tuple[str, ...], abi: list[str]
) -> None:
    compiler = shutil.which("g++")
    if compiler is None:
        pytest.skip("GCC is required for the embedded ABI compile probe")
    if abi and (platform.system() != "Linux" or platform.machine() != "x86_64"):
        pytest.skip("32-bit ABI probe requires an x86_64 Linux host")
    source = tmp_path / "size_types.cpp"
    source.write_text(
        "".join(f"#include <{header}>\n" for header in headers)
        + 'static_assert(__is_same(size_t, fl::size_t), "size_t mismatch");\n'
        + 'static_assert(__is_same(ptrdiff_t, fl::ptrdiff_t), "ptrdiff_t mismatch");\n',
        encoding="utf-8",
    )
    result = RunningProcess.run(
        [
            compiler,
            *abi,
            "-std=c++17",
            "-fsyntax-only",
            "-I",
            str(ROOT / "src"),
            str(source),
        ],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        check=False,
        timeout=30,
    )
    assert result.returncode == 0, result.stdout
