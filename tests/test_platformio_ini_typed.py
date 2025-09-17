#!/usr/bin/env python3
"""
Unit tests for the typed PlatformIO configuration parsing.
"""

import unittest
from pathlib import Path
import tempfile
import os
import sys

from ci.compiler.platformio_ini import (
    PlatformIOIni,
    PlatformIOSection,
    EnvironmentSection,
    GlobalEnvSection,
    ParsedPlatformIOConfig,
    _parse_list_value,
    _resolve_variable_substitution,
)


class TestListValueParsing(unittest.TestCase):
    """Test the list value parsing functionality."""

    def test_empty_value(self):
        """Test parsing empty values."""
        self.assertEqual(_parse_list_value(""), [])
        self.assertEqual(_parse_list_value("   "), [])

    def test_single_value(self):
        """Test parsing single values."""
        self.assertEqual(_parse_list_value("single_value"), ["single_value"])
        self.assertEqual(_parse_list_value("  spaced_value  "), ["spaced_value"])

    def test_comma_separated(self):
        """Test parsing comma-separated values."""
        result = _parse_list_value("value1, value2, value3")
        self.assertEqual(result, ["value1", "value2", "value3"])

    def test_multiline_values(self):
        """Test parsing multi-line values."""
        multiline = """
    -DDEBUG
    -DPIN_DATA=9
    -DPIN_CLOCK=7
        """
        result = _parse_list_value(multiline)
        self.assertEqual(result, ["-DDEBUG", "-DPIN_DATA=9", "-DPIN_CLOCK=7"])

    def test_mixed_empty_lines(self):
        """Test parsing multi-line values with empty lines."""
        multiline = """
    value1

    value2
    value3
        """
        result = _parse_list_value(multiline)
        self.assertEqual(result, ["value1", "value2", "value3"])


class TestVariableSubstitution(unittest.TestCase):
    """Test variable substitution functionality."""

    def setUp(self):
        """Set up test configuration."""
        import configparser
        self.config = configparser.ConfigParser()
        self.config.add_section('platformio')
        self.config.set('platformio', 'build_cache_dir', '.pio_cache')
        self.config.add_section('env:base')
        self.config.set('env:base', 'build_flags', '-DBASE_FLAG=1')

    def test_no_substitution(self):
        """Test values without variable substitution."""
        result = _resolve_variable_substitution("normal_value", self.config)
        self.assertEqual(result, "normal_value")

    def test_simple_substitution(self):
        """Test simple variable substitution."""
        result = _resolve_variable_substitution("${platformio.build_cache_dir}", self.config)
        self.assertEqual(result, ".pio_cache")

    def test_env_substitution(self):
        """Test environment variable substitution."""
        result = _resolve_variable_substitution("${env:base.build_flags}", self.config)
        self.assertEqual(result, "-DBASE_FLAG=1")

    def test_mixed_substitution(self):
        """Test mixed content with substitution."""
        result = _resolve_variable_substitution("prefix_${platformio.build_cache_dir}_suffix", self.config)
        self.assertEqual(result, "prefix_.pio_cache_suffix")

    def test_missing_variable(self):
        """Test substitution with missing variable."""
        result = _resolve_variable_substitution("${missing.variable}", self.config)
        self.assertEqual(result, "${missing.variable}")  # Should remain unchanged


class TestPlatformIOSectionParsing(unittest.TestCase):
    """Test parsing of [platformio] section."""

    def test_empty_platformio_section(self):
        """Test parsing empty [platformio] section."""
        config_str = """
[platformio]
"""
        pio_ini = PlatformIOIni.parseString(config_str)
        section = pio_ini.get_platformio_section()

        self.assertIsNotNone(section)
        self.assertIsNone(section.src_dir)
        self.assertIsNone(section.default_envs)

    def test_platformio_section_with_values(self):
        """Test parsing [platformio] section with values."""
        config_str = """
[platformio]
src_dir = custom_src
default_envs = env1, env2
description = Test project
build_cache_dir = .custom_cache
"""
        pio_ini = PlatformIOIni.parseString(config_str)
        section = pio_ini.get_platformio_section()

        self.assertIsNotNone(section)
        self.assertEqual(section.src_dir, "custom_src")
        self.assertEqual(section.default_envs, ["env1", "env2"])
        self.assertEqual(section.description, "Test project")
        self.assertEqual(section.build_cache_dir, ".custom_cache")

    def test_no_platformio_section(self):
        """Test config without [platformio] section."""
        config_str = """
[env:test]
platform = test
"""
        pio_ini = PlatformIOIni.parseString(config_str)
        section = pio_ini.get_platformio_section()

        self.assertIsNone(section)


class TestEnvironmentSectionParsing(unittest.TestCase):
    """Test parsing of environment sections."""

    def test_simple_environment(self):
        """Test parsing simple environment section."""
        config_str = """
[env:test_env]
platform = test_platform
board = test_board
framework = arduino
"""
        pio_ini = PlatformIOIni.parseString(config_str)
        env = pio_ini.get_environment("test_env")

        self.assertIsNotNone(env)
        self.assertEqual(env.name, "test_env")
        self.assertEqual(env.platform, "test_platform")
        self.assertEqual(env.board, "test_board")
        self.assertEqual(env.framework, "arduino")

    def test_environment_with_lists(self):
        """Test parsing environment with list values."""
        config_str = """
[env:test_env]
platform = test_platform
build_flags =
    -DDEBUG
    -DTEST_FLAG=1
lib_deps =
    Library1
    Library2
    Library3
"""
        pio_ini = PlatformIOIni.parseString(config_str)
        env = pio_ini.get_environment("test_env")

        self.assertIsNotNone(env)
        self.assertEqual(env.build_flags, ["-DDEBUG", "-DTEST_FLAG=1"])
        self.assertEqual(env.lib_deps, ["Library1", "Library2", "Library3"])

    def test_environment_with_custom_options(self):
        """Test parsing environment with custom options."""
        config_str = """
[env:test_env]
platform = test_platform
custom_option1 = custom_value1
custom_option2 = custom_value2
board_build.partitions = custom.csv
"""
        pio_ini = PlatformIOIni.parseString(config_str)
        env = pio_ini.get_environment("test_env")

        self.assertIsNotNone(env)
        self.assertEqual(env.platform, "test_platform")
        self.assertIn("custom_option1", env.custom_options)
        self.assertIn("custom_option2", env.custom_options)
        self.assertEqual(env.custom_options["custom_option1"], "custom_value1")
        self.assertEqual(env.custom_options["custom_option2"], "custom_value2")
        # board_build.partitions is a known field
        self.assertEqual(env.board_build_partitions, "custom.csv")


class TestGlobalEnvironmentParsing(unittest.TestCase):
    """Test parsing of global [env] section."""

    def test_global_env_section(self):
        """Test parsing global environment section."""
        config_str = """
[env]
extra_scripts = pre:global_script.py
lib_deps = GlobalLib
monitor_speed = 115200

[env:test_env]
platform = test_platform
"""
        pio_ini = PlatformIOIni.parseString(config_str)
        global_env = pio_ini.get_global_env_section()
        test_env = pio_ini.get_environment("test_env")

        self.assertIsNotNone(global_env)
        self.assertEqual(global_env.extra_scripts, ["pre:global_script.py"])
        self.assertEqual(global_env.lib_deps, ["GlobalLib"])
        self.assertEqual(global_env.monitor_speed, "115200")

        # Test that global settings are applied to environment
        self.assertIsNotNone(test_env)
        self.assertIn("GlobalLib", test_env.lib_deps)
        self.assertIn("pre:global_script.py", test_env.extra_scripts)
        self.assertEqual(test_env.monitor_speed, "115200")


class TestInheritance(unittest.TestCase):
    """Test environment inheritance functionality."""

    def test_simple_inheritance(self):
        """Test simple environment inheritance."""
        config_str = """
[env:base]
platform = base_platform
framework = arduino
build_flags = -DBASE_FLAG=1

[env:child]
extends = env:base
board = child_board
build_flags = -DCHILD_FLAG=1
"""
        pio_ini = PlatformIOIni.parseString(config_str)
        child_env = pio_ini.get_environment("child")

        self.assertIsNotNone(child_env)
        self.assertEqual(child_env.platform, "base_platform")  # Inherited
        self.assertEqual(child_env.framework, "arduino")       # Inherited
        self.assertEqual(child_env.board, "child_board")       # Own value
        # Build flags should be merged (base + child)
        self.assertIn("-DBASE_FLAG=1", child_env.build_flags)
        self.assertIn("-DCHILD_FLAG=1", child_env.build_flags)

    def test_chain_inheritance(self):
        """Test chain inheritance (A extends B extends C)."""
        config_str = """
[env:grandparent]
platform = gp_platform
build_flags = -DGP_FLAG=1

[env:parent]
extends = env:grandparent
framework = arduino
build_flags = -DPARENT_FLAG=1

[env:child]
extends = env:parent
board = child_board
build_flags = -DCHILD_FLAG=1
"""
        pio_ini = PlatformIOIni.parseString(config_str)
        child_env = pio_ini.get_environment("child")

        self.assertIsNotNone(child_env)
        self.assertEqual(child_env.platform, "gp_platform")    # From grandparent
        self.assertEqual(child_env.framework, "arduino")       # From parent
        self.assertEqual(child_env.board, "child_board")       # Own value

        # All build flags should be present
        self.assertIn("-DGP_FLAG=1", child_env.build_flags)
        self.assertIn("-DPARENT_FLAG=1", child_env.build_flags)
        self.assertIn("-DCHILD_FLAG=1", child_env.build_flags)

    def test_inheritance_with_global_env(self):
        """Test inheritance combined with global environment."""
        config_str = """
[env]
extra_scripts = global_script.py
lib_deps = GlobalLib

[env:base]
platform = base_platform
lib_deps = BaseLib

[env:child]
extends = env:base
board = child_board
lib_deps = ChildLib
"""
        pio_ini = PlatformIOIni.parseString(config_str)
        child_env = pio_ini.get_environment("child")

        self.assertIsNotNone(child_env)
        # Should have all libraries: global + base + child
        expected_libs = ["GlobalLib", "BaseLib", "ChildLib"]
        for lib in expected_libs:
            self.assertIn(lib, child_env.lib_deps)

        # Should have global extra scripts
        self.assertIn("global_script.py", child_env.extra_scripts)


class TestVariableSubstitutionIntegration(unittest.TestCase):
    """Test variable substitution in full configuration."""

    def test_variable_substitution_in_build_flags(self):
        """Test variable substitution in build flags."""
        config_str = """
[platformio]
build_cache_dir = .pio_cache

[env:base]
build_flags = -DBASE_FLAG=1
    -DCACHE_DIR=${platformio.build_cache_dir}

[env:child]
extends = env:base
build_flags = ${env:base.build_flags}
    -DCHILD_FLAG=1
"""
        pio_ini = PlatformIOIni.parseString(config_str)
        child_env = pio_ini.get_environment("child")

        self.assertIsNotNone(child_env)
        # Should resolve platformio variable
        self.assertIn("-DCACHE_DIR=.pio_cache", child_env.build_flags)
        # Should have base flags
        self.assertIn("-DBASE_FLAG=1", child_env.build_flags)
        # Should have child flags
        self.assertIn("-DCHILD_FLAG=1", child_env.build_flags)


class TestDumpAllAttributes(unittest.TestCase):
    """Test the dump_all_attributes functionality."""

    def test_dump_complete_config(self):
        """Test dumping all attributes from complete config."""
        config_str = """
[platformio]
src_dir = test_src
default_envs = env1

[env]
extra_scripts = global_script.py

[env:env1]
platform = test_platform
board = test_board
"""
        pio_ini = PlatformIOIni.parseString(config_str)
        all_attrs = pio_ini.dump_all_attributes()

        self.assertIn('platformio', all_attrs)
        self.assertIn('env', all_attrs)
        self.assertIn('environments', all_attrs)

        # Check platformio section
        self.assertEqual(all_attrs['platformio']['src_dir'], 'test_src')
        self.assertEqual(all_attrs['platformio']['default_envs'], ['env1'])

        # Check global env section
        self.assertEqual(all_attrs['env']['extra_scripts'], ['global_script.py'])

        # Check environments
        self.assertIn('env1', all_attrs['environments'])
        env1_data = all_attrs['environments']['env1']
        self.assertEqual(env1_data['platform'], 'test_platform')
        self.assertEqual(env1_data['board'], 'test_board')


class TestRealFileIntegration(unittest.TestCase):
    """Test integration with real platformio.ini files."""

    def test_main_platformio_ini(self):
        """Test parsing the main platformio.ini file."""
        main_file = Path("platformio.ini")
        if not main_file.exists():
            self.skipTest("Main platformio.ini not found")

        pio_ini = PlatformIOIni.parseFile(main_file)

        # Should have platformio section
        platformio_section = pio_ini.get_platformio_section()
        self.assertIsNotNone(platformio_section)

        # Should have environments
        environments = pio_ini.get_all_environments()
        self.assertGreater(len(environments), 0)

        # Should be able to dump all attributes
        all_attrs = pio_ini.dump_all_attributes()
        self.assertIn('environments', all_attrs)

    def test_kitchensink_platformio_ini(self):
        """Test parsing the kitchensink platformio.ini file."""
        kitchensink_file = Path("ci/kitchensink/platformio.ini")
        if not kitchensink_file.exists():
            self.skipTest("Kitchensink platformio.ini not found")

        pio_ini = PlatformIOIni.parseFile(kitchensink_file)

        # Should have platformio section
        platformio_section = pio_ini.get_platformio_section()
        self.assertIsNotNone(platformio_section)

        # Should have at least one environment
        environments = pio_ini.get_all_environments()
        self.assertGreater(len(environments), 0)


class TestErrorHandling(unittest.TestCase):
    """Test error handling and edge cases."""

    def test_missing_extended_environment(self):
        """Test handling of missing extended environment."""
        config_str = """
[env:child]
extends = env:nonexistent
platform = test_platform
"""
        pio_ini = PlatformIOIni.parseString(config_str)
        child_env = pio_ini.get_environment("child")

        # Should still parse successfully
        self.assertIsNotNone(child_env)
        self.assertEqual(child_env.platform, "test_platform")

    def test_invalid_config_string(self):
        """Test handling of invalid configuration string."""
        with self.assertRaises(Exception):
            PlatformIOIni.parseString("[invalid section without closing bracket")

    def test_nonexistent_environment(self):
        """Test getting non-existent environment."""
        config_str = """
[env:existing]
platform = test_platform
"""
        pio_ini = PlatformIOIni.parseString(config_str)
        nonexistent = pio_ini.get_environment("nonexistent")

        self.assertIsNone(nonexistent)

    def test_cache_invalidation(self):
        """Test that cache is invalidated when config changes."""
        config_str = """
[env:test]
platform = original_platform
"""
        pio_ini = PlatformIOIni.parseString(config_str)

        # Get environment first time
        env1 = pio_ini.get_environment("test")
        self.assertEqual(env1.platform, "original_platform")

        # Modify configuration
        pio_ini.set_option("env:test", "platform", "modified_platform")

        # Should get updated value
        env2 = pio_ini.get_environment("test")
        self.assertEqual(env2.platform, "modified_platform")


if __name__ == '__main__':
    unittest.main()