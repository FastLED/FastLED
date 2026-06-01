from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from ci import wasm_flags


def test_lib_debug_flags_map_fastled_src_to_dwarf_prefix() -> None:
    flags = wasm_flags.get_lib_compile_flags_dict("debug")["compiler_flags"]
    source_root = (wasm_flags.PROJECT_ROOT / "src").resolve()

    assert f"-ffile-prefix-map={source_root.as_posix()}=fastledsource" in flags
    if str(source_root) != source_root.as_posix():
        assert f"-ffile-prefix-map={source_root}=fastledsource" in flags
    assert "-ffile-prefix-map=/=sketchsource/" not in flags


def test_lib_fast_debug_flags_map_fastled_src_to_dwarf_prefix() -> None:
    flags = wasm_flags.get_lib_compile_flags_dict("fast_debug")["compiler_flags"]
    source_root = (wasm_flags.PROJECT_ROOT / "src").resolve()

    assert f"-ffile-prefix-map={source_root.as_posix()}=fastledsource" in flags


def test_lib_quick_flags_do_not_emit_dwarf_prefix_maps() -> None:
    flags = wasm_flags.get_lib_compile_flags_dict("quick")["compiler_flags"]

    for flag in flags:
        assert not flag.startswith("-ffile-prefix-map="), flag


def test_rglob_outputs_forward_slashes(tmp_path: Path) -> None:
    source_dir = tmp_path / "src"
    nested = source_dir / "platforms" / "shared"
    nested.mkdir(parents=True)
    (nested / "demo.cpp").write_text("int demo = 0;\n", encoding="utf-8")

    result = subprocess.run(
        [
            sys.executable,
            str(wasm_flags.PROJECT_ROOT / "ci" / "meson" / "rglob.py"),
            str(source_dir),
            "*.cpp",
        ],
        check=True,
        capture_output=True,
        text=True,
    )

    assert "\\" not in result.stdout
    assert "/platforms/shared/demo.cpp" in result.stdout
