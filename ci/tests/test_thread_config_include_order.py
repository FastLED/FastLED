"""WASM threading selectors must agree across independently compiled units."""

import shutil
import sys
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
        assert result.returncode == 0, result.stdout


@pytest.mark.skipif(
    sys.platform == "win32",
    reason="WASM's long-based size_t cannot emulate the Windows host STL ABI",
)
def test_wasm_singleton_backing_storage_preserves_atomic_mutex_alignment(
    tmp_path: Path,
) -> None:
    compiler = shutil.which("clang++") or shutil.which("g++") or shutil.which("c++")
    if compiler is None:
        pytest.skip("No host C++ compiler is available")

    # compiler_control.h needs the export annotation from the SDK. The host
    # fixture exercises no JavaScript calls or other Emscripten APIs.
    (tmp_path / "emscripten.h").write_text(
        "#define EMSCRIPTEN_KEEPALIVE __attribute__((used))\n", encoding="utf-8"
    )
    source = tmp_path / "singleton_alignment.cpp"
    source.write_text(
        """
#include "fl/stl/singleton.h"
#include "platforms/shared/atomic.h"
#include <mutex>

struct State {
    std::mutex mutex;
    fl::AtomicReal<unsigned int> counter{0};
};
// Check the exact storage contract used by Singleton rather than relying on
// a linker accidentally placing a byte-aligned global at an aligned address.
struct FL_ALIGN_AS_T(alignof(State)) Storage { char data[sizeof(State)]; };
static_assert(alignof(Storage) >= alignof(State), "singleton backing alignment");

int main() {
    fl::Singleton<char, 1>::instance() = 1;
    State& first = fl::Singleton<State, 1>::instance();
    fl::Singleton<char, 2>::instance() = 2;
    State& second = fl::Singleton<State, 2>::instance();
    if (reinterpret_cast<fl::uptr>(&first) % alignof(State) ||
        reinterpret_cast<fl::uptr>(&second) % alignof(State)) return 1;
    first.mutex.lock();
    first.counter.fetch_add(1);
    first.mutex.unlock();
    second.mutex.lock();
    second.counter.fetch_add(1);
    second.mutex.unlock();
    return first.counter.load() == 1 && second.counter.load() == 1 ? 0 : 2;
}
""",
        encoding="utf-8",
    )
    executable = tmp_path / "singleton_alignment"
    result = RunningProcess.run(
        [
            compiler,
            "-std=c++17",
            "-pthread",
            "-D__EMSCRIPTEN__",
            "-DFASTLED_MULTITHREADED=0",
            f"-I{tmp_path}",
            f"-I{ROOT / 'src'}",
            str(source),
            "-o",
            str(executable),
        ],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
        timeout=120,
    )
    assert result.returncode == 0, result.stdout
    result = RunningProcess.run(
        [str(executable)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
        timeout=120,
    )
    assert result.returncode == 0, result.stdout
