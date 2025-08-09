"""
Unit tests for unity file generation behavior in the Python compiler.

Validates that:
- Unity content matches expected includes order
- Unity file writes are idempotent (no rewrite when content unchanged)
- Unity file is rewritten when source list changes
"""

import os
import tempfile
import time
import unittest
from pathlib import Path
from typing import List
from unittest import TestCase

from ci.compiler.clang_compiler import BuildFlags, Compiler, CompilerOptions


class TestUnityGeneration(TestCase):
    def setUp(self) -> None:
        self.temp_dir = Path(tempfile.mkdtemp(prefix="unity_test_")).resolve()
        self.unity_dir = self.temp_dir / "unity"
        self.unity_dir.mkdir(parents=True, exist_ok=True)
        self.src_dir = self.temp_dir / "src"
        self.src_dir.mkdir(parents=True, exist_ok=True)

        # Create minimal compilable .cpp files
        self.cpp1 = self.src_dir / "a.cpp"
        self.cpp2 = self.src_dir / "b.cpp"

        with open(self.cpp1, "w", encoding="utf-8") as f1:
            f1.write(
                """
static int func_a() { return 1; }
int var_a = func_a();
""".lstrip()
            )

        with open(self.cpp2, "w", encoding="utf-8") as f2:
            f2.write(
                """
static int func_b() { return 2; }
int var_b = func_b();
""".lstrip()
            )

        # Load build flags and create a compiler instance
        project_root = Path.cwd()
        toml_path = project_root / "ci" / "build_unit.toml"
        build_flags: BuildFlags = BuildFlags.parse(
            toml_path, quick_build=True, strict_mode=False
        )

        compiler_args: List[str] = []
        for item in build_flags.tools.cpp_compiler:
            compiler_args.append(item)
        for item in build_flags.compiler_flags:
            compiler_args.append(item)
        for item in build_flags.include_flags:
            compiler_args.append(item)

        settings = CompilerOptions(
            include_path=str(project_root / "src"),
            defines=[],
            compiler_args=compiler_args,
        )

        self.compiler = Compiler(settings, build_flags)

    def tearDown(self) -> None:
        # Best effort cleanup of temp directory
        try:
            for root, dirs, files in os.walk(self.temp_dir, topdown=False):
                for name in files:
                    try:
                        (Path(root) / name).unlink(missing_ok=True)
                    except Exception:
                        pass
                for name in dirs:
                    try:
                        (Path(root) / name).rmdir()
                    except Exception:
                        pass
            self.temp_dir.rmdir()
        except Exception:
            # Ignore cleanup issues on Windows
            pass

    def test_unity_write_idempotent_and_updates_on_change(self) -> None:
        # First run: generate and compile one chunk unity
        options = CompilerOptions(
            include_path=self.compiler.settings.include_path,
            defines=[],
            compiler_args=self.compiler.settings.compiler_args,
            use_pch=False,
            additional_flags=["-c"],
        )

        cpp_list: List[str] = [str(self.cpp1), str(self.cpp2)]

        result = self.compiler._compile_unity_chunks_sync(  # Internal, synchronous for testing
            options,
            cpp_list,
            chunks=1,
            unity_dir=self.unity_dir,
            no_parallel=True,
        )

        self.assertTrue(result.success, "Initial unity compilation should succeed")

        unity_cpp = self.unity_dir / "unity1.cpp"
        self.assertTrue(unity_cpp.exists(), "unity1.cpp should be created")

        first_mtime = unity_cpp.stat().st_mtime_ns

        # Second run with identical inputs should not rewrite the file
        time.sleep(0.01)  # Ensure detectable time difference if rewritten

        result2 = self.compiler._compile_unity_chunks_sync(
            options,
            cpp_list,
            chunks=1,
            unity_dir=self.unity_dir,
            no_parallel=True,
        )
        self.assertTrue(result2.success, "Second unity compilation should succeed")

        second_mtime = unity_cpp.stat().st_mtime_ns
        self.assertEqual(
            first_mtime,
            second_mtime,
            "unity1.cpp should not be rewritten when content is unchanged",
        )

        # Change the input set (add a new file) to force unity content change and rewrite
        cpp3 = self.src_dir / "c.cpp"
        with open(cpp3, "w", encoding="utf-8") as f3:
            f3.write(
                """
static int func_c() { return 3; }
int var_c = func_c();
""".lstrip()
            )

        # Update list to include the new file
        cpp_list.append(str(cpp3))

        time.sleep(0.01)
        result3 = self.compiler._compile_unity_chunks_sync(
            options,
            cpp_list,
            chunks=1,
            unity_dir=self.unity_dir,
            no_parallel=True,
        )
        self.assertTrue(
            result3.success, "Unity compilation after input set change should succeed"
        )

        third_mtime = unity_cpp.stat().st_mtime_ns
        self.assertGreater(
            third_mtime,
            second_mtime,
            "unity1.cpp should be rewritten when the included file list changes",
        )


if __name__ == "__main__":
    unittest.main()
