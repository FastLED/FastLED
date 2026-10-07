"""WASM threading selectors must agree across independently compiled units."""

import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize("override", [None, 0, 1])
def test_wasm_atomic_mode_is_independent_of_platform_include_order(
    tmp_path: Path, override: int | None
) -> None:
    compiler = shutil.which("clang++") or shutil.which("g++") or shutil.which("c++")
    if compiler is None:
        pytest.skip("No host C++ compiler is available")

    # Model only the WASM toolchain's pthread-header availability. AtomicReal
    # uses compiler builtins and needs no pthread declarations in this test.
    (tmp_path / "pthread.h").write_text("", encoding="utf-8")
    expected = 1 if override is None else override
    for platform_first in (False, True):
        source = tmp_path / f"order_{platform_first}.cpp"
        prefix = '#include "platforms/is_platform.h"\n' if platform_first else ""
        source.write_text(
            prefix
            + '#include "fl/stl/atomic.h"\n'
            + '#include "platforms/is_platform.h"\n'
            + f'static_assert(FASTLED_MULTITHREADED == {expected}, "thread mode");\n'
            + "static_assert(FASTLED_USE_REAL_ATOMICS == "
            + f'{expected}, "atomic mode");\n',
            encoding="utf-8",
        )
        command = [
            compiler,
            "-std=c++17",
            "-fsyntax-only",
            "-D__EMSCRIPTEN__",
            f"-I{tmp_path}",
            f"-I{ROOT / 'src'}",
        ]
        if override is not None:
            command.append(f"-DFASTLED_MULTITHREADED={override}")
        command.append(str(source))
        result = RunningProcess.run(
            command,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
            timeout=120,
        )
        assert result.returncode == 0, result.stderr
