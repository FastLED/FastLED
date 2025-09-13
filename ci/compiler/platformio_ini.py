#!/usr/bin/env python3
"""
PlatformIO INI file parser and writer.

This module provides a clean interface for parsing, manipulating, and writing
platformio.ini files. It's designed to be a general-purpose utility that can
be used by various tools that need to work with PlatformIO configuration files.
"""

import configparser
import io
import logging
import os
import re
import tempfile
import uuid
from dataclasses import dataclass, field
from pathlib import Path
from typing import TYPE_CHECKING, Any, Callable, Dict, List, Optional, Tuple, Union


if TYPE_CHECKING:
    from ci.compiler.platformio_cache import PlatformIOCache


# Configure logging
logger = logging.getLogger(__name__)


@dataclass
class PlatformIOSection:
    """
    Typed representation of the [platformio] section.
    Contains generic project settings and directory options.
    """

    # Generic Options
    name: Optional[str] = None
    description: Optional[str] = None
    default_envs: Optional[List[str]] = None
    extra_configs: Optional[List[str]] = None

    # Directory Options
    core_dir: Optional[str] = None
    globallib_dir: Optional[str] = None
    platforms_dir: Optional[str] = None
    packages_dir: Optional[str] = None
    cache_dir: Optional[str] = None
    build_cache_dir: Optional[str] = None
    workspace_dir: Optional[str] = None
    build_dir: Optional[str] = None
    libdeps_dir: Optional[str] = None
    include_dir: Optional[str] = None
    src_dir: Optional[str] = None
    lib_dir: Optional[str] = None
    data_dir: Optional[str] = None
    test_dir: Optional[str] = None
    boards_dir: Optional[str] = None
    monitor_dir: Optional[str] = None
    shared_dir: Optional[str] = None


@dataclass
class EnvironmentSection:
    """
    Typed representation of an [env:*] section.
    Contains environment-specific build and configuration settings.
    """

    # Required/Core Options
    name: str = ""  # The environment name (e.g., "esp32s3" from "env:esp32s3")

    # Platform Options
    platform: Optional[str] = None
    framework: Optional[str] = None
    board: Optional[str] = None

    # Inheritance
    extends: Optional[str] = None

    # Build Options
    build_type: Optional[str] = None
    build_flags: List[str] = field(default_factory=lambda: [])
    build_src_filter: List[str] = field(default_factory=lambda: [])
    targets: List[str] = field(default_factory=lambda: [])

    # Library Options
    lib_deps: List[str] = field(default_factory=lambda: [])
    lib_ignore: List[str] = field(default_factory=lambda: [])
    lib_extra_dirs: List[str] = field(default_factory=lambda: [])
    lib_ldf_mode: Optional[str] = None

    # Upload Options
    upload_port: Optional[str] = None
    upload_protocol: Optional[str] = None
    upload_speed: Optional[str] = None

    # Monitor Options
    monitor_port: Optional[str] = None
    monitor_speed: Optional[str] = None
    monitor_filters: List[str] = field(default_factory=lambda: [])

    # Board-specific Options
    board_build_mcu: Optional[str] = None
    board_build_f_cpu: Optional[str] = None
    board_build_partitions: Optional[str] = None

    # Extra Scripts and Tools
    extra_scripts: List[str] = field(default_factory=lambda: [])
    check_tool: Optional[str] = None

    # Custom Options (for non-standard options)
    custom_options: Dict[str, str] = field(default_factory=lambda: {})


@dataclass
class GlobalEnvSection:
    """
    Typed representation of the [env] section.
    Contains global settings that apply to all environments.
    """

    # Build Options
    build_flags: List[str] = field(default_factory=lambda: [])
    build_src_filter: List[str] = field(default_factory=lambda: [])

    # Library Options
    lib_deps: List[str] = field(default_factory=lambda: [])
    lib_ignore: List[str] = field(default_factory=lambda: [])
    lib_extra_dirs: List[str] = field(default_factory=lambda: [])
    lib_ldf_mode: Optional[str] = None

    # Monitor Options
    monitor_speed: Optional[str] = None
    monitor_filters: List[str] = field(default_factory=lambda: [])

    # Extra Scripts
    extra_scripts: List[str] = field(default_factory=lambda: [])

    # Custom Options (for non-standard options)
    custom_options: Dict[str, str] = field(default_factory=lambda: {})


@dataclass
class ParsedPlatformIOConfig:
    """
    Complete typed representation of a parsed platformio.ini file.
    """

    platformio_section: Optional[PlatformIOSection] = None
    global_env_section: Optional[GlobalEnvSection] = None
    environments: Dict[str, EnvironmentSection] = field(default_factory=lambda: {})


def _resolve_variable_substitution(
    value: str, config: configparser.ConfigParser
) -> str:
    """
    Resolve variable substitution in a configuration value.
    Supports patterns like ${env:generic-esp.build_flags} and ${platformio.build_cache_dir}.

    Args:
        value: Value potentially containing variable references
        config: ConfigParser to resolve variables from

    Returns:
        Value with variables resolved
    """
    if not value or "${" not in value:
        return value

    # Pattern to match ${section.option} or ${section:subsection.option}
    pattern = r"\$\{([^}]+)\}"

    def replace_variable(match: re.Match[str]) -> str:
        var_ref = match.group(1)

        # Handle section:subsection.option format (like env:generic-esp.build_flags)
        if ":" in var_ref:
            section_part, option = var_ref.split(".", 1)
            section = section_part.replace(":", ":")
        else:
            # Handle section.option format (like platformio.build_cache_dir)
            section, option = var_ref.split(".", 1)

        try:
            if config.has_option(section, option):
                return config.get(section, option)
            else:
                # Return original if not found
                return match.group(0)
        except (configparser.NoSectionError, configparser.NoOptionError):
            # Return original if resolution fails
            return match.group(0)

    return re.sub(pattern, replace_variable, value)


def _resolve_list_variables(
    values: List[str], config: configparser.ConfigParser
) -> List[str]:
    """
    Resolve variable substitution in a list of values.
    """
    resolved: List[str] = []
    for value in values:
        resolved_value = _resolve_variable_substitution(value, config)
        # If the resolved value contains newlines or commas, parse it as a list
        if resolved_value != value and (
            "\n" in resolved_value or "," in resolved_value
        ):
            resolved.extend(_parse_list_value(resolved_value))
        else:
            resolved.append(resolved_value)
    return resolved


def _parse_list_value(value: str) -> List[str]:
    """
    Parse a configuration value that can be either comma-separated or multi-line.

    Args:
        value: Raw configuration value

    Returns:
        List of parsed values
    """
    if not value or not value.strip():
        return []

    # Handle multi-line format (values on separate lines)
    if "\n" in value:
        lines = [line.strip() for line in value.split("\n") if line.strip()]
        return lines

    # Handle comma-separated format
    if "," in value:
        return [item.strip() for item in value.split(",") if item.strip()]

    # Single value
    return [value.strip()] if value.strip() else []


def _parse_platformio_section(
    config: configparser.ConfigParser,
) -> Optional[PlatformIOSection]:
    """
    Parse the [platformio] section into a typed dataclass.
    """
    if not config.has_section("platformio"):
        return None

    section = config["platformio"]

    # Parse default_envs and extra_configs as lists
    default_envs = None
    if "default_envs" in section:
        default_envs = _parse_list_value(section["default_envs"])

    extra_configs = None
    if "extra_configs" in section:
        extra_configs = _parse_list_value(section["extra_configs"])

    return PlatformIOSection(
        name=section.get("name"),
        description=section.get("description"),
        default_envs=default_envs,
        extra_configs=extra_configs,
        core_dir=section.get("core_dir"),
        globallib_dir=section.get("globallib_dir"),
        platforms_dir=section.get("platforms_dir"),
        packages_dir=section.get("packages_dir"),
        cache_dir=section.get("cache_dir"),
        build_cache_dir=section.get("build_cache_dir"),
        workspace_dir=section.get("workspace_dir"),
        build_dir=section.get("build_dir"),
        libdeps_dir=section.get("libdeps_dir"),
        include_dir=section.get("include_dir"),
        src_dir=section.get("src_dir"),
        lib_dir=section.get("lib_dir"),
        data_dir=section.get("data_dir"),
        test_dir=section.get("test_dir"),
        boards_dir=section.get("boards_dir"),
        monitor_dir=section.get("monitor_dir"),
        shared_dir=section.get("shared_dir"),
    )


def _merge_env_sections(
    base_env: EnvironmentSection, child_env: EnvironmentSection
) -> EnvironmentSection:
    """
    Merge a base environment section with a child that extends it.
    Child values override base values, except for lists which are merged.
    """
    from dataclasses import replace

    # Start with the child environment
    merged = replace(child_env)

    # Override with base values only if child doesn't have them
    if not merged.platform and base_env.platform:
        merged.platform = base_env.platform
    if not merged.framework and base_env.framework:
        merged.framework = base_env.framework
    if not merged.board and base_env.board:
        merged.board = base_env.board
    if not merged.build_type and base_env.build_type:
        merged.build_type = base_env.build_type
    if not merged.lib_ldf_mode and base_env.lib_ldf_mode:
        merged.lib_ldf_mode = base_env.lib_ldf_mode
    if not merged.upload_port and base_env.upload_port:
        merged.upload_port = base_env.upload_port
    if not merged.upload_protocol and base_env.upload_protocol:
        merged.upload_protocol = base_env.upload_protocol
    if not merged.upload_speed and base_env.upload_speed:
        merged.upload_speed = base_env.upload_speed
    if not merged.monitor_port and base_env.monitor_port:
        merged.monitor_port = base_env.monitor_port
    if not merged.monitor_speed and base_env.monitor_speed:
        merged.monitor_speed = base_env.monitor_speed
    if not merged.board_build_mcu and base_env.board_build_mcu:
        merged.board_build_mcu = base_env.board_build_mcu
    if not merged.board_build_f_cpu and base_env.board_build_f_cpu:
        merged.board_build_f_cpu = base_env.board_build_f_cpu
    if not merged.board_build_partitions and base_env.board_build_partitions:
        merged.board_build_partitions = base_env.board_build_partitions
    if not merged.check_tool and base_env.check_tool:
        merged.check_tool = base_env.check_tool

    # For list fields, prepend base values to child values (child has priority)
    if base_env.build_flags:
        merged.build_flags = base_env.build_flags + merged.build_flags
    if base_env.build_src_filter:
        merged.build_src_filter = base_env.build_src_filter + merged.build_src_filter
    if base_env.targets:
        merged.targets = base_env.targets + merged.targets
    if base_env.lib_deps:
        merged.lib_deps = base_env.lib_deps + merged.lib_deps
    if base_env.lib_ignore:
        merged.lib_ignore = base_env.lib_ignore + merged.lib_ignore
    if base_env.lib_extra_dirs:
        merged.lib_extra_dirs = base_env.lib_extra_dirs + merged.lib_extra_dirs
    if base_env.monitor_filters:
        merged.monitor_filters = base_env.monitor_filters + merged.monitor_filters
    if base_env.extra_scripts:
        merged.extra_scripts = base_env.extra_scripts + merged.extra_scripts

    # Merge custom options (child overrides base)
    if base_env.custom_options:
        merged_custom = base_env.custom_options.copy()
        merged_custom.update(merged.custom_options)
        merged.custom_options = merged_custom

    return merged


def _parse_env_section(
    config: configparser.ConfigParser, section_name: str
) -> EnvironmentSection:
    """
    Parse an environment section into a typed dataclass.
    """
    section = config[section_name]

    # Extract environment name from section name (e.g., "env:esp32s3" -> "esp32s3")
    env_name = section_name[4:] if section_name.startswith("env:") else section_name

    # Parse list-based options with variable resolution
    build_flags = _resolve_list_variables(
        _parse_list_value(section.get("build_flags", "")), config
    )
    build_src_filter = _resolve_list_variables(
        _parse_list_value(section.get("build_src_filter", "")), config
    )
    targets = _resolve_list_variables(
        _parse_list_value(section.get("targets", "")), config
    )
    lib_deps = _resolve_list_variables(
        _parse_list_value(section.get("lib_deps", "")), config
    )
    lib_ignore = _resolve_list_variables(
        _parse_list_value(section.get("lib_ignore", "")), config
    )
    lib_extra_dirs = _resolve_list_variables(
        _parse_list_value(section.get("lib_extra_dirs", "")), config
    )
    monitor_filters = _resolve_list_variables(
        _parse_list_value(section.get("monitor_filters", "")), config
    )
    extra_scripts = _resolve_list_variables(
        _parse_list_value(section.get("extra_scripts", "")), config
    )

    # Handle custom options (anything not in standard fields)
    standard_options = {
        "platform",
        "framework",
        "board",
        "extends",
        "build_type",
        "build_flags",
        "build_src_filter",
        "targets",
        "lib_deps",
        "lib_ignore",
        "lib_extra_dirs",
        "lib_ldf_mode",
        "upload_port",
        "upload_protocol",
        "upload_speed",
        "monitor_port",
        "monitor_speed",
        "monitor_filters",
        "board_build.mcu",
        "board_build.f_cpu",
        "board_build.partitions",
        "extra_scripts",
        "check_tool",
        "custom_sdkconfig",  # Add known custom options
    }

    custom_options: Dict[str, str] = {}
    for key, value in section.items():
        if key not in standard_options:
            custom_options[key] = value

    return EnvironmentSection(
        name=env_name,
        platform=_resolve_variable_substitution(section.get("platform", ""), config)
        or None,
        framework=_resolve_variable_substitution(section.get("framework", ""), config)
        or None,
        board=_resolve_variable_substitution(section.get("board", ""), config) or None,
        extends=section.get("extends"),
        build_type=section.get("build_type"),
        build_flags=build_flags,
        build_src_filter=build_src_filter,
        targets=targets,
        lib_deps=lib_deps,
        lib_ignore=lib_ignore,
        lib_extra_dirs=lib_extra_dirs,
        lib_ldf_mode=section.get("lib_ldf_mode"),
        upload_port=section.get("upload_port"),
        upload_protocol=section.get("upload_protocol"),
        upload_speed=section.get("upload_speed"),
        monitor_port=section.get("monitor_port"),
        monitor_speed=section.get("monitor_speed"),
        monitor_filters=monitor_filters,
        board_build_mcu=section.get("board_build.mcu"),
        board_build_f_cpu=section.get("board_build.f_cpu"),
        board_build_partitions=section.get("board_build.partitions"),
        extra_scripts=extra_scripts,
        check_tool=section.get("check_tool"),
        custom_options=custom_options,
    )


def _parse_global_env_section(
    config: configparser.ConfigParser,
) -> Optional[GlobalEnvSection]:
    """
    Parse the [env] section into a typed dataclass.
    """
    if not config.has_section("env"):
        return None

    section = config["env"]

    # Parse list-based options
    build_flags = _parse_list_value(section.get("build_flags", ""))
    build_src_filter = _parse_list_value(section.get("build_src_filter", ""))
    lib_deps = _parse_list_value(section.get("lib_deps", ""))
    lib_ignore = _parse_list_value(section.get("lib_ignore", ""))
    lib_extra_dirs = _parse_list_value(section.get("lib_extra_dirs", ""))
    monitor_filters = _parse_list_value(section.get("monitor_filters", ""))
    extra_scripts = _parse_list_value(section.get("extra_scripts", ""))

    # Handle custom options
    standard_options = {
        "build_flags",
        "build_src_filter",
        "lib_deps",
        "lib_ignore",
        "lib_extra_dirs",
        "lib_ldf_mode",
        "monitor_speed",
        "monitor_filters",
        "extra_scripts",
    }

    custom_options: Dict[str, str] = {}
    for key, value in section.items():
        if key not in standard_options:
            custom_options[key] = value

    return GlobalEnvSection(
        build_flags=build_flags,
        build_src_filter=build_src_filter,
        lib_deps=lib_deps,
        lib_ignore=lib_ignore,
        lib_extra_dirs=lib_extra_dirs,
        lib_ldf_mode=section.get("lib_ldf_mode"),
        monitor_speed=section.get("monitor_speed"),
        monitor_filters=monitor_filters,
        extra_scripts=extra_scripts,
        custom_options=custom_options,
    )


class PlatformIOIni:
    """
    A class for parsing and dumping platformio.ini files.
    Provides a clean interface for managing INI file operations.

    Use static factory methods to create instances:
    - PlatformIOIni.parseFile(file_path) - Parse from file
    - PlatformIOIni.parseString(content) - Parse from string
    - PlatformIOIni.create() - Create empty instance
    """

    config: configparser.ConfigParser
    file_path: Optional[Path]
    _parsed_config: Optional[ParsedPlatformIOConfig] = None

    @staticmethod
    def parseFile(file_path: Path) -> "PlatformIOIni":
        """
        Static factory method to parse a platformio.ini file and return a PlatformIOIni instance.

        Args:
            file_path: Path to the platformio.ini file

        Returns:
            PlatformIOIni instance with the parsed configuration

        Raises:
            FileNotFoundError: If the file doesn't exist
            configparser.Error: If the file is malformed
        """
        instance = object.__new__(PlatformIOIni)
        instance.config = configparser.ConfigParser()
        instance.file_path = file_path

        if not file_path.exists():
            raise FileNotFoundError(f"platformio.ini not found: {file_path}")

        try:
            instance.config.read(file_path)
            instance._parsed_config = None  # Will be lazily parsed
            logger.debug(f"Successfully parsed platformio.ini: {file_path}")
        except configparser.Error as e:
            logger.error(f"Error parsing platformio.ini: {e}")
            raise

        return instance

    @staticmethod
    def parseString(content: str) -> "PlatformIOIni":
        """
        Parse platformio.ini content from a string and return a fully formed instance.

        Args:
            content: INI content as string

        Returns:
            PlatformIOIni instance with the parsed configuration

        Raises:
            configparser.Error: If the content is malformed
        """
        instance = object.__new__(PlatformIOIni)
        instance.config = configparser.ConfigParser()
        instance.file_path = None

        try:
            instance.config.read_string(content)
            instance._parsed_config = None  # Will be lazily parsed
            logger.debug("Successfully parsed platformio.ini from string")
        except configparser.Error as e:
            logger.error(f"Error parsing platformio.ini from string: {e}")
            raise

        return instance

    @staticmethod
    def create() -> "PlatformIOIni":
        """
        Static factory method to create an empty PlatformIOIni instance.

        Returns:
            Empty PlatformIOIni instance
        """
        instance = object.__new__(PlatformIOIni)
        instance.config = configparser.ConfigParser()
        instance.file_path = None
        instance._parsed_config = None
        return instance

    def dump(self, file_path: Path) -> None:
        """
        Write the configuration to a platformio.ini file.

        Args:
            file_path: Path to write the configuration to
        """

        # Write atomically using a temporary file
        temp_file = file_path.with_suffix(".tmp")
        try:
            with open(temp_file, "w", encoding="utf-8") as f:
                self.config.write(f)
            temp_file.replace(file_path)
            logger.debug(f"Successfully wrote platformio.ini: {file_path}")
        except Exception as e:
            if temp_file.exists():
                temp_file.unlink()
            logger.error(f"Failed to write platformio.ini: {e}")
            raise

    def get_sections(self) -> List[str]:
        """
        Get all section names in the configuration.

        Returns:
            List of section names
        """
        return self.config.sections()

    def get_env_sections(self) -> List[str]:
        """
        Get all environment section names (sections starting with 'env:').

        Returns:
            List of environment section names
        """
        return [
            section for section in self.config.sections() if section.startswith("env:")
        ]

    def has_section(self, section: str) -> bool:
        """
        Check if a section exists.

        Args:
            section: Section name to check

        Returns:
            True if section exists, False otherwise
        """
        return self.config.has_section(section)

    def has_option(self, section: str, option: str) -> bool:
        """
        Check if an option exists in a section.

        Args:
            section: Section name
            option: Option name to check

        Returns:
            True if option exists in section, False otherwise
        """
        return self.config.has_option(section, option)

    def get_option(
        self, section: str, option: str, fallback: Optional[str] = None
    ) -> Optional[str]:
        """
        Get an option value from a section.

        Args:
            section: Section name
            option: Option name
            fallback: Default value if option doesn't exist

        Returns:
            Option value or fallback if not found
        """
        if not self.config.has_section(section):
            return fallback
        return self.config.get(section, option, fallback=fallback)

    def set_option(self, section: str, option: str, value: str) -> None:
        """
        Set an option value in a section.

        Args:
            section: Section name
            option: Option name
            value: Option value
        """
        if not self.config.has_section(section):
            self.config.add_section(section)
        self.config.set(section, option, value)
        self.invalidate_cache()  # Invalidate cache when config changes

    def remove_option(self, section: str, option: str) -> bool:
        """
        Remove an option from a section.

        Args:
            section: Section name
            option: Option name

        Returns:
            True if option was removed, False if it didn't exist
        """
        result = self.config.remove_option(section, option)
        if result:
            self.invalidate_cache()  # Invalidate cache when config changes
        return result

    def get_platform_urls(self) -> List[Tuple[str, str, str]]:
        """
        Get all platform URLs from environment sections.

        Returns:
            List of tuples: (section_name, option_name, url)
        """
        urls: List[Tuple[str, str, str]] = []
        for section in self.get_env_sections():
            platform_value = self.get_option(section, "platform")
            if platform_value:
                urls.append((section, "platform", platform_value))
        return urls

    def get_framework_urls(self) -> List[Tuple[str, str, str]]:
        """
        Get all framework URLs from environment sections.

        Returns:
            List of tuples: (section_name, option_name, url)
        """
        urls: List[Tuple[str, str, str]] = []
        for section in self.get_env_sections():
            framework_value = self.get_option(section, "framework")
            if framework_value:
                urls.append((section, "framework", framework_value))
        return urls

    def replace_url(
        self, section: str, option: str, old_url: str, new_url: str
    ) -> bool:
        """
        Replace a URL in the configuration.

        Args:
            section: Section name
            option: Option name
            old_url: URL to replace
            new_url: New URL value

        Returns:
            True if replacement was made, False otherwise
        """
        replacement_made = False
        current_value = self.get_option(section, option)
        if current_value == old_url:
            self.set_option(section, option, new_url)
            replacement_made = True
        return replacement_made

    def validate_structure(self) -> List[str]:
        """
        Validate the platformio.ini structure and return any issues.

        Returns:
            List of validation issues (empty if valid)
        """
        issues: List[str] = []

        # Check for at least one environment section
        env_sections = self.get_env_sections()
        if not env_sections:
            issues.append(
                "No environment sections found (sections starting with 'env:')"
            )

        # Validate each environment section
        for section in env_sections:
            # Check for required fields
            if not self.has_option(section, "platform"):
                issues.append(f"Section '{section}' missing required 'platform' option")

            # Check for board specification
            if not self.has_option(section, "board"):
                issues.append(f"Section '{section}' missing 'board' option")

        return issues

    def to_dict(self) -> Dict[str, Dict[str, str]]:
        """
        Convert the configuration to a dictionary.

        Returns:
            Dictionary representation of the configuration
        """
        result: Dict[str, Dict[str, str]] = {}
        for section_name in self.config.sections():
            result[section_name] = dict(self.config[section_name])
        return result

    def from_dict(self, data: Dict[str, Dict[str, str]]) -> None:
        """
        Load configuration from a dictionary.

        Args:
            data: Dictionary with section names as keys and option dictionaries as values
        """
        self.config.clear()
        self._parsed_config = None  # Invalidate parsed cache
        for section_name, options in data.items():
            self.config.add_section(section_name)
            for option, value in options.items():
                self.config.set(section_name, option, value)

    @property
    def parsed(self) -> ParsedPlatformIOConfig:
        """
        Get the parsed, typed configuration. This property is lazily computed and cached.

        Returns:
            ParsedPlatformIOConfig with all sections parsed into typed dataclasses
        """
        if self._parsed_config is None:
            self._parsed_config = self._parse_config()
        return self._parsed_config

    def _parse_config(self) -> ParsedPlatformIOConfig:
        """
        Parse the raw configparser data into typed dataclasses.

        Returns:
            ParsedPlatformIOConfig with all sections parsed
        """
        # Parse [platformio] section
        platformio_section = _parse_platformio_section(self.config)

        # Parse [env] section
        global_env_section = _parse_global_env_section(self.config)

        # Parse all [env:*] sections
        environments: Dict[str, EnvironmentSection] = {}
        for section_name in self.get_env_sections():
            env_name = section_name[4:]  # Remove "env:" prefix
            environments[env_name] = _parse_env_section(self.config, section_name)

        # Resolve inheritance (extends)
        resolved_environments: Dict[str, EnvironmentSection] = {}
        for env_name, env_section in environments.items():
            resolved_env = self._resolve_inheritance(
                env_section, environments, global_env_section
            )
            resolved_environments[env_name] = resolved_env

        return ParsedPlatformIOConfig(
            platformio_section=platformio_section,
            global_env_section=global_env_section,
            environments=resolved_environments,
        )

    def _resolve_inheritance(
        self,
        env_section: EnvironmentSection,
        all_environments: Dict[str, EnvironmentSection],
        global_env: Optional[GlobalEnvSection],
    ) -> EnvironmentSection:
        """
        Resolve inheritance for an environment section.
        """
        if not env_section.extends:
            # Apply global environment settings if no inheritance
            return self._apply_global_env(env_section, global_env)

        # Parse extends value (remove "env:" prefix if present)
        base_env_name = env_section.extends
        if base_env_name.startswith("env:"):
            base_env_name = base_env_name[4:]

        # Get the base environment
        if base_env_name not in all_environments:
            logger.warning(
                f"Environment '{env_section.name}' extends non-existent environment '{base_env_name}'"
            )
            return self._apply_global_env(env_section, global_env)

        # Recursively resolve base environment inheritance
        base_env = self._resolve_inheritance(
            all_environments[base_env_name], all_environments, global_env
        )

        # Merge base and child environments
        merged = _merge_env_sections(base_env, env_section)
        return merged

    def _apply_global_env(
        self, env_section: EnvironmentSection, global_env: Optional[GlobalEnvSection]
    ) -> EnvironmentSection:
        """
        Apply global environment settings to an environment section.
        """
        if not global_env:
            return env_section

        from dataclasses import replace

        # Apply global settings where environment doesn't have them
        merged = replace(env_section)

        # String fields
        if not merged.lib_ldf_mode and global_env.lib_ldf_mode:
            merged.lib_ldf_mode = global_env.lib_ldf_mode
        if not merged.monitor_speed and global_env.monitor_speed:
            merged.monitor_speed = global_env.monitor_speed

        # List fields - prepend global values
        if global_env.build_flags:
            merged.build_flags = global_env.build_flags + merged.build_flags
        if global_env.build_src_filter:
            merged.build_src_filter = (
                global_env.build_src_filter + merged.build_src_filter
            )
        if global_env.lib_deps:
            merged.lib_deps = global_env.lib_deps + merged.lib_deps
        if global_env.lib_ignore:
            merged.lib_ignore = global_env.lib_ignore + merged.lib_ignore
        if global_env.lib_extra_dirs:
            merged.lib_extra_dirs = global_env.lib_extra_dirs + merged.lib_extra_dirs
        if global_env.monitor_filters:
            merged.monitor_filters = global_env.monitor_filters + merged.monitor_filters
        if global_env.extra_scripts:
            merged.extra_scripts = global_env.extra_scripts + merged.extra_scripts

        # Custom options
        if global_env.custom_options:
            merged_custom = global_env.custom_options.copy()
            merged_custom.update(merged.custom_options)
            merged.custom_options = merged_custom

        return merged

    def get_platformio_section(self) -> Optional[PlatformIOSection]:
        """
        Get the typed [platformio] section.

        Returns:
            PlatformIOSection instance or None if section doesn't exist
        """
        return self.parsed.platformio_section

    def get_global_env_section(self) -> Optional[GlobalEnvSection]:
        """
        Get the typed [env] section.

        Returns:
            GlobalEnvSection instance or None if section doesn't exist
        """
        return self.parsed.global_env_section

    def get_environment(self, env_name: str) -> Optional[EnvironmentSection]:
        """
        Get a typed environment section by name.

        Args:
            env_name: Environment name (without "env:" prefix)

        Returns:
            EnvironmentSection instance or None if environment doesn't exist
        """
        return self.parsed.environments.get(env_name)

    def get_all_environments(self) -> Dict[str, EnvironmentSection]:
        """
        Get all typed environment sections.

        Returns:
            Dictionary mapping environment names to EnvironmentSection instances
        """
        return self.parsed.environments.copy()

    def dump_all_attributes(self) -> Dict[str, Any]:
        """
        Dump all parsed attributes as a dictionary for inspection.

        Returns:
            Dictionary containing all parsed configuration data
        """
        from dataclasses import asdict

        parsed = self.parsed
        result: Dict[str, Any] = {}

        if parsed.platformio_section:
            result["platformio"] = asdict(parsed.platformio_section)

        if parsed.global_env_section:
            result["env"] = asdict(parsed.global_env_section)

        result["environments"] = {}
        for env_name, env_section in parsed.environments.items():
            result["environments"][env_name] = asdict(env_section)

        return result

    def invalidate_cache(self) -> None:
        """
        Invalidate the parsed configuration cache.
        Call this method after making changes to the underlying configparser.
        """
        self._parsed_config = None

    def __str__(self) -> str:
        """Return string representation of the configuration."""
        output = io.StringIO()
        self.config.write(output)
        return output.getvalue()

    def optimize(self, cache: "PlatformIOCache") -> None:
        """
        Download all packages and swap URLs for local file path URLs.

        This method will:
        1. Find all platform and framework URLs that point to zip files
        2. Download and cache them using the provided PlatformIO cache system
        3. Replace the URLs in-place with local file:// URLs

        Args:
            cache: PlatformIOCache instance to use for downloading and caching packages.
        """
        # Import here to avoid circular dependencies
        from ci.compiler.platformio_cache import _is_zip_web_url, handle_zip_artifact

        cache_manager = cache

        # Find all platform and framework URLs that need optimization
        zip_artifacts: List[Tuple[str, bool, str]] = []

        # Scan platform URLs
        for section_name, option_name, url in self.get_platform_urls():
            if _is_zip_web_url(url):
                zip_artifacts.append((url, False, section_name))  # False = platform

        # Scan framework URLs
        for section_name, option_name, url in self.get_framework_urls():
            if _is_zip_web_url(url):
                zip_artifacts.append((url, True, section_name))  # True = framework

        # Deduplicate artifacts by URL to avoid redundant processing
        unique_artifacts: Dict[
            str, Tuple[bool, str]
        ] = {}  # url -> (is_framework, env_section)
        for artifact_url, is_framework, env_section in zip_artifacts:
            if artifact_url not in unique_artifacts:
                unique_artifacts[artifact_url] = (is_framework, env_section)

        # Process each unique artifact and collect URL replacements
        replacements: Dict[str, str] = {}
        for artifact_url, (is_framework, env_section) in unique_artifacts.items():
            resolved_path = handle_zip_artifact(
                artifact_url, cache_manager, env_section
            )
            replacements[artifact_url] = resolved_path

        # Apply replacements to the configuration
        if replacements:
            # Replace platform URLs
            for section_name, option_name, url in self.get_platform_urls():
                if url in replacements:
                    self.replace_url(section_name, option_name, url, replacements[url])

            # Replace framework URLs
            for section_name, option_name, url in self.get_framework_urls():
                if url in replacements:
                    self.replace_url(section_name, option_name, url, replacements[url])
