"""Link the production entry point to verify weak defaults and archive behavior."""

import shutil
import sys
from pathlib import Path

import pytest
from running_process import RunningProcess


SRC = Path(__file__).resolve().parents[2] / "src"
MARKER = b"FASTLED_NATIVE_FS_FALLBACK_4889"


@pytest.mark.parametrize("mode", ["absent", "archive", "direct"])
@pytest.mark.skipif(sys.platform != "linux", reason="ELF linker GC probe")
def test_embedded_fs_provider_linkage(tmp_path: Path, mode: str) -> None:
    compiler = shutil.which("c++") or shutil.which("clang++")
    archive = shutil.which("ar")
    if compiler is None or archive is None:
        pytest.skip("C++ compiler and ELF archiver required")

    def run(arguments: list[str]) -> None:
        result = RunningProcess.run(
            arguments,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=60,
        )
        assert result.returncode == 0, result.stdout

    include = tmp_path / "include"
    dispatcher = include / "platforms/embedded_fs.h"
    dispatcher.parent.mkdir(parents=True)
    # Replace only SDK dispatch. The actual getEmbeddedFs and weak default
    # implementation are compiled below; native VFS behavior has device tests.
    dispatcher.write_text(
        '#include "fl/fs/backend.h"\n'
        "extern int nativeCalls;\nextern const char* volatile nativeMarker;\n"
        "namespace fl { namespace platforms {\n"
        "inline FsImplPtr makeEmbeddedFs(bool) FL_NO_EXCEPT {\n"
        ' ++nativeCalls; nativeMarker="FASTLED_NATIVE_FS_FALLBACK_4889"; return {};\n}}}\n'
    )
    entry = tmp_path / "entry.cpp"
    entry.write_text(
        '#include "fl/fs/embedded/embedded_fs.h"\n'
        "#define FL_IS_ESP32 1\n"
        '#include "fl/fs/embedded/embedded_fs.cpp.hpp"\n'
    )
    provider = tmp_path / "provider.cpp"
    provider.write_text(
        '#include "platforms/embedded_fs_provider.h"\n'
        "namespace fl { namespace platforms {\n"
        "FsImplPtr fastled_embedded_fs_provider(bool format) FL_NO_EXCEPT {\n"
        " if (!format) __builtin_trap();\n"
        " return FsImplPtr();\n}}}\n"
    )
    main = tmp_path / "main.cpp"
    expected = 0 if mode == "direct" else 1
    main.write_text(
        '#include "fl/stl/shared_ptr.cpp.hpp"\n'
        '#include "fl/fs/embedded/embedded_fs.h"\n'
        "int nativeCalls = 0;\nconst char* volatile nativeMarker = nullptr;\n"
        "int main() {\n"
        " auto fs = fl::getEmbeddedFs(true);\n"
        f" return nativeCalls == {expected} ? 0 : 1;\n"
        "}\n"
    )
    flags = [
        compiler,
        "-std=c++11",
        "-Os",
        "-ffunction-sections",
        "-fdata-sections",
        f"-I{include}",
        f"-I{SRC}",
    ]
    entry_obj = tmp_path / "entry.o"
    provider_obj = tmp_path / "provider.o"
    run([*flags, "-c", str(entry), "-o", str(entry_obj)])
    run([*flags, "-c", str(provider), "-o", str(provider_obj)])
    optional = tmp_path / "liboptional.a"
    run([archive, "rcs", str(optional), str(provider_obj)])
    dependencies = (
        [str(provider_obj)]
        if mode == "direct"
        else [str(optional)]
        if mode == "archive"
        else []
    )
    executable = tmp_path / "probe"
    run(
        [
            *flags,
            str(main),
            str(entry_obj),
            *dependencies,
            "-Wl,--gc-sections",
            "-o",
            str(executable),
        ]
    )
    run([str(executable)])
    assert (MARKER in executable.read_bytes()) == (mode != "direct")
