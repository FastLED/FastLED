#!/usr/bin/env python3
"""
Unit tests for BuildFlags TOML parsing and serialization functionality.

Tests the BuildFlags and BuildTools classes for parsing TOML build configuration
files and serializing back to TOML format.
"""

import tempfile
import unittest
from pathlib import Path

from ci.compiler.clang_compiler import ArchiveOptions, BuildFlags, BuildTools


class TestBuildFlagsToml(unittest.TestCase):
    """Test BuildFlags TOML parsing and serialization"""

    def setUp(self) -> None:
        """Set up test fixtures"""
        self.temp_dir = Path(tempfile.mkdtemp())

    def tearDown(self) -> None:
        """Clean up test fixtures"""
        # Clean up any test files
        for file in self.temp_dir.glob("*.toml"):
            file.unlink()
        self.temp_dir.rmdir()

    def create_test_toml(self, content: str) -> Path:
        """Create a test TOML file with the given content"""
        test_file = self.temp_dir / "test_build_flags.toml"
        with open(test_file, "w", encoding="utf-8") as f:
            f.write(content)
        return test_file

    def test_build_tools_defaults(self) -> None:
        """Test BuildTools default values"""
        tools = BuildTools(
            cpp_compiler=[],
            archiver=[],
            linker=[],
            c_compiler=[],
            objcopy=[],
            nm=[],
            strip=[],
            ranlib=[],
        )

        # Test modern command-based fields (should be empty by default)
        self.assertEqual(tools.cpp_compiler, [])
        self.assertEqual(tools.archiver, [])
        self.assertEqual(tools.linker, [])

        # Test other important fields
        self.assertEqual(tools.c_compiler, [])
        self.assertEqual(tools.objcopy, [])
        self.assertEqual(tools.nm, [])
        self.assertEqual(tools.strip, [])
        self.assertEqual(tools.ranlib, [])

    def test_parse_minimal_toml(self) -> None:
        """Test parsing minimal TOML file with required tools section"""
        toml_content = """
[all]
defines = ["-DTEST=1"]
compiler_flags = ["-Wall"]
include_flags = ["-I."]

[tools]
cpp_compiler = ["uv", "run", "python", "-m", "ziglang", "c++"]
linker = ["uv", "run", "python", "-m", "ziglang", "c++"]
c_compiler = ["clang"]
objcopy = ["uv", "run", "python", "-m", "ziglang", "objcopy"]
nm = ["uv", "run", "python", "-m", "ziglang", "nm"]
strip = ["uv", "run", "python", "-m", "ziglang", "strip"]
ranlib = ["uv", "run", "python", "-m", "ziglang", "ranlib"]
archiver = ["uv", "run", "python", "-m", "ziglang", "ar"]

[archive]
flags = "rcsD"

[linking.base]
flags = ["-pthread"]

[strict_mode]
flags = ["-Werror"]
"""

        test_file = self.create_test_toml(toml_content)
        flags = BuildFlags.parse(test_file, quick_build=False, strict_mode=False)

        # Check basic flags
        self.assertEqual(flags.defines, ["-DTEST=1"])
        self.assertEqual(flags.compiler_flags, ["-Wall"])
        self.assertEqual(flags.include_flags, ["-I."])
        self.assertEqual(flags.link_flags, ["-pthread"])
        self.assertEqual(flags.strict_mode_flags, ["-Werror"])

        # Check tools (from [tools] section) - modern command-based approach
        self.assertEqual(flags.tools.c_compiler, ["clang"])
        self.assertEqual(
            flags.tools.linker, ["uv", "run", "python", "-m", "ziglang", "c++"]
        )
        self.assertEqual(
            flags.tools.cpp_compiler,
            ["uv", "run", "python", "-m", "ziglang", "c++"],
        )
        self.assertEqual(
            flags.tools.objcopy, ["uv", "run", "python", "-m", "ziglang", "objcopy"]
        )
        self.assertEqual(flags.tools.nm, ["uv", "run", "python", "-m", "ziglang", "nm"])
        self.assertEqual(
            flags.tools.strip, ["uv", "run", "python", "-m", "ziglang", "strip"]
        )
        self.assertEqual(
            flags.tools.ranlib, ["uv", "run", "python", "-m", "ziglang", "ranlib"]
        )
        self.assertEqual(
            flags.tools.archiver, ["uv", "run", "python", "-m", "ziglang", "ar"]
        )

    def test_parse_toml_with_tools(self) -> None:
        """Test parsing TOML file with [tools] section"""
        toml_content = """
[all]
defines = ["-DTEST=1"]
compiler_flags = ["-Wall"]
include_flags = ["-I."]

[tools]
cpp_compiler = ["g++"]
archiver = ["gcc-ar"]
linker = ["ld.gold"]
c_compiler = ["gcc"]
objcopy = ["arm-objcopy"]
nm = ["arm-nm"]
strip = ["arm-strip"]
ranlib = ["arm-ranlib"]

[archive]
flags = "rcsD"

[linking.base]
flags = ["-pthread"]
"""

        test_file = self.create_test_toml(toml_content)
        flags = BuildFlags.parse(test_file, quick_build=False, strict_mode=False)

        # Check that tools were parsed correctly - modern command-based approach
        self.assertEqual(flags.tools.cpp_compiler, ["g++"])
        self.assertEqual(flags.tools.archiver, ["gcc-ar"])
        self.assertEqual(flags.tools.linker, ["ld.gold"])
        self.assertEqual(flags.tools.c_compiler, ["gcc"])
        self.assertEqual(flags.tools.objcopy, ["arm-objcopy"])
        self.assertEqual(flags.tools.nm, ["arm-nm"])
        self.assertEqual(flags.tools.strip, ["arm-strip"])
        self.assertEqual(flags.tools.ranlib, ["arm-ranlib"])

    def test_parse_partial_tools_section(self) -> None:
        """Test parsing TOML with all required [tools] section fields"""
        toml_content = """
[all]
defines = ["-DTEST=1"]

[tools]
cpp_compiler = ["custom-clang++"]
archiver = ["custom-ar"]
linker = ["custom-linker"]
c_compiler = ["clang"]
objcopy = ["custom-objcopy"]
nm = ["custom-nm"]
strip = ["custom-strip"]
ranlib = ["custom-ranlib"]
# All tools must be provided - no defaults allowed

[archive]
flags = "rcsD"
"""

        test_file = self.create_test_toml(toml_content)
        flags = BuildFlags.parse(test_file, quick_build=False, strict_mode=False)

        # Check all tools are set as specified - strict validation
        self.assertEqual(flags.tools.cpp_compiler, ["custom-clang++"])
        self.assertEqual(flags.tools.archiver, ["custom-ar"])
        self.assertEqual(flags.tools.linker, ["custom-linker"])
        self.assertEqual(flags.tools.c_compiler, ["clang"])
        self.assertEqual(flags.tools.objcopy, ["custom-objcopy"])
        self.assertEqual(flags.tools.nm, ["custom-nm"])
        self.assertEqual(flags.tools.strip, ["custom-strip"])
        self.assertEqual(flags.tools.ranlib, ["custom-ranlib"])

    def test_serialize_build_flags_with_tools(self) -> None:
        """Test serializing BuildFlags with tools to TOML"""
        # Create BuildFlags with custom tools
        custom_tools = BuildTools(
            linker=["arm-none-eabi-ld"],
            c_compiler=["arm-none-eabi-gcc"],
            objcopy=["arm-none-eabi-objcopy"],
            nm=["arm-none-eabi-nm"],
            strip=["arm-none-eabi-strip"],
            ranlib=["arm-none-eabi-ranlib"],
            cpp_compiler=["arm-none-eabi-g++"],
            archiver=["arm-none-eabi-ar"],
        )

        flags = BuildFlags(
            defines=["-DARM_BUILD=1"],
            compiler_flags=["-mcpu=cortex-m4", "-mthumb"],
            include_flags=["-I.", "-Iarm"],
            link_flags=["-nostdlib"],
            strict_mode_flags=["-Werror"],
            tools=custom_tools,
            archive=ArchiveOptions(flags="rcsD"),
        )

        # Serialize to TOML
        toml_output = flags.serialize()

        # Check that tools section is present with new field names
        self.assertIn("[tools]", toml_output)
        self.assertIn("cpp_compiler = ['arm-none-eabi-g++']", toml_output)
        self.assertIn("archiver = ['arm-none-eabi-ar']", toml_output)
        self.assertIn("linker = ['arm-none-eabi-ld']", toml_output)
        self.assertIn("c_compiler = ['arm-none-eabi-gcc']", toml_output)
        self.assertIn("objcopy = ['arm-none-eabi-objcopy']", toml_output)
        self.assertIn("nm = ['arm-none-eabi-nm']", toml_output)
        self.assertIn("strip = ['arm-none-eabi-strip']", toml_output)
        self.assertIn("ranlib = ['arm-none-eabi-ranlib']", toml_output)

    def test_serialize_with_none_linker(self) -> None:
        """Test serializing BuildFlags when linker is None"""
        flags = BuildFlags(
            defines=["-DTEST=1"],
            compiler_flags=[],
            include_flags=[],
            link_flags=[],
            strict_mode_flags=[],
            tools=BuildTools(
                linker=[],  # Empty list instead of None
                cpp_compiler=["clang++"],
                c_compiler=["clang"],
                archiver=[],
                objcopy=[],
                nm=[],
                strip=[],
                ranlib=[],
            ),
            archive=ArchiveOptions(flags="rcsD"),
        )

        toml_output = flags.serialize()

        # Check that tools section is present but linker is omitted
        self.assertIn("[tools]", toml_output)
        self.assertIn("cpp_compiler = ['clang++']", toml_output)
        self.assertNotIn("linker =", toml_output)  # Should be omitted when empty

    def test_round_trip_toml_parsing(self) -> None:
        """Test that parse -> serialize -> parse maintains data integrity"""
        # Create original flags
        original_tools = BuildTools(
            linker=["test-ld"],
            c_compiler=["test-gcc"],
            cpp_compiler=["test-compiler"],
            archiver=["test-ar"],
            objcopy=["test-objcopy"],
            nm=["test-nm"],
            strip=["test-strip"],
            ranlib=["test-ranlib"],
        )

        original_flags = BuildFlags(
            defines=["-DROUND_TRIP=1"],
            compiler_flags=["-Wall", "-O2"],
            include_flags=["-I.", "-Itest"],
            link_flags=["-pthread"],
            strict_mode_flags=["-Werror"],
            tools=original_tools,
            archive=ArchiveOptions(flags="rcsD"),
        )

        # Serialize to TOML
        toml_content = original_flags.serialize()

        # Write to temporary file
        temp_file = self.temp_dir / "roundtrip.toml"
        original_flags.to_toml_file(temp_file)

        # Parse back from file
        parsed_flags = BuildFlags.parse(temp_file, quick_build=False, strict_mode=False)

        # Check that all data is preserved
        self.assertEqual(parsed_flags.defines, original_flags.defines)
        self.assertEqual(parsed_flags.compiler_flags, original_flags.compiler_flags)
        self.assertEqual(parsed_flags.include_flags, original_flags.include_flags)
        self.assertEqual(parsed_flags.link_flags, original_flags.link_flags)
        self.assertEqual(
            parsed_flags.strict_mode_flags, original_flags.strict_mode_flags
        )

        # Check tools - modern command-based approach
        self.assertEqual(
            parsed_flags.tools.cpp_compiler, original_flags.tools.cpp_compiler
        )
        self.assertEqual(parsed_flags.tools.archiver, original_flags.tools.archiver)
        self.assertEqual(parsed_flags.tools.linker, original_flags.tools.linker)
        self.assertEqual(parsed_flags.tools.c_compiler, original_flags.tools.c_compiler)

    def test_parse_missing_file(self) -> None:
        """Test parsing non-existent TOML file raises FileNotFoundError"""
        nonexistent_file = self.temp_dir / "does_not_exist.toml"

        # Should raise FileNotFoundError when file is missing
        with self.assertRaises(FileNotFoundError) as context:
            BuildFlags.parse(nonexistent_file, quick_build=False, strict_mode=False)

        self.assertIn("Required build_flags.toml not found", str(context.exception))

    def test_from_toml_file_alias(self) -> None:
        """Test that from_toml_file() is an alias for parse()"""
        toml_content = """
[all]
defines = ["-DALIAS_TEST=1"]

[tools]
cpp_compiler = ["alias-compiler"]
linker = ["alias-linker"]
c_compiler = ["clang"]
objcopy = ["alias-objcopy"]
nm = ["alias-nm"]
strip = ["alias-strip"]
ranlib = ["alias-ranlib"]
archiver = ["alias-archiver"]

[archive]
flags = "rcsD"
"""

        test_file = self.create_test_toml(toml_content)

        # Parse using both methods
        flags_parse = BuildFlags.parse(test_file, quick_build=True, strict_mode=False)
        flags_alias = BuildFlags.from_toml_file(
            test_file, quick_build=True, strict_mode=False
        )

        # Should be identical
        self.assertEqual(flags_parse.defines, flags_alias.defines)
        self.assertEqual(flags_parse.tools.cpp_compiler, flags_alias.tools.cpp_compiler)
        self.assertEqual(flags_parse.tools.cpp_compiler, ["alias-compiler"])


if __name__ == "__main__":
    unittest.main()
