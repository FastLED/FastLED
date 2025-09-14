#!/usr/bin/env python3
"""
PlatformIO INI file parser and writer.

This module provides a clean interface for parsing, manipulating, and writing
platformio.ini files. It's designed to be a general-purpose utility that can
be used by various tools that need to work with PlatformIO configuration files.
"""

import configparser
import io
import json
import logging
import os
import re
import subprocess
import tempfile
import time
import uuid
from dataclasses import dataclass, field
from datetime import datetime, timedelta
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


@dataclass
class PackageInfo:
    """
    Information about a package/dependency.
    """

    name: str
    type: str = ""
    requirements: str = ""
    url: str = ""
    optional: bool = True
    version: Optional[str] = None
    description: Optional[str] = None


@dataclass
class PlatformUrlResolution:
    """
    Multi-value resolution result for a platform with different URL types.
    """

    name: str
    git_url: Optional[str] = None
    zip_url: Optional[str] = None
    local_path: Optional[str] = None
    version: Optional[str] = None
    frameworks: List[str] = field(default_factory=lambda: [])
    packages: List[PackageInfo] = field(default_factory=lambda: [])
    homepage: Optional[str] = None

    @property
    def preferred_url(self) -> Optional[str]:
        """Get the preferred URL for downloading (zip preferred for speed)."""
        return self.zip_url or self.git_url

    @property
    def has_downloadable_url(self) -> bool:
        """Check if there's a URL available for downloading."""
        return bool(self.git_url or self.zip_url)


@dataclass
class FrameworkUrlResolution:
    """
    Multi-value resolution result for a framework with different URL types.
    """

    name: str
    git_url: Optional[str] = None
    zip_url: Optional[str] = None
    local_path: Optional[str] = None
    homepage: Optional[str] = None
    platforms: List[str] = field(default_factory=lambda: [])
    version: Optional[str] = None
    title: Optional[str] = None
    description: Optional[str] = None
    url: Optional[str] = None  # Original URL from CLI response

    @property
    def preferred_url(self) -> Optional[str]:
        """Get the preferred URL for downloading (zip preferred for speed)."""
        return self.zip_url or self.git_url or self.url


@dataclass
class PlatformShowResponse:
    """
    Strongly typed representation of PlatformIO 'pio platform show' command response.
    """

    name: str
    title: Optional[str] = None
    version: Optional[str] = None
    repository: Optional[str] = None
    frameworks: List[str] = field(default_factory=list)  # type: ignore
    packages: List[Dict[str, Any]] = field(  # type: ignore[reportUnknownVariableType]
        default_factory=list
    )  # Raw package data from CLI
    homepage: Optional[str] = None
    description: Optional[str] = None

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "PlatformShowResponse":
        """Create PlatformShowResponse from raw CLI JSON response."""
        return cls(
            name=data.get("name", ""),
            title=data.get("title"),
            version=data.get("version"),
            repository=data.get("repository"),
            frameworks=data.get("frameworks", []),
            packages=data.get("packages", []),
            homepage=data.get("homepage"),
            description=data.get("description"),
        )


@dataclass
class FrameworkInfo:
    """
    Strongly typed representation of framework information from PlatformIO CLI.
    """

    name: str
    title: Optional[str] = None
    description: Optional[str] = None
    url: Optional[str] = None
    homepage: Optional[str] = None
    platforms: List[str] = field(default_factory=list)  # type: ignore
    version: Optional[str] = None

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "FrameworkInfo":
        """Create FrameworkInfo from raw CLI JSON response."""
        return cls(
            name=data.get("name", ""),
            title=data.get("title"),
            description=data.get("description"),
            url=data.get("url"),
            homepage=data.get("homepage"),
            platforms=data.get("platforms", []),
            version=data.get("version"),
        )


@dataclass
class PlatformCacheEntry:
    """
    Strongly typed representation of a platform cache entry for inspection.
    """

    repository_url: Optional[str]
    version: Optional[str]
    frameworks: List[str]
    resolved_at: Optional[str]
    expires_at: Optional[str]


@dataclass
class FrameworkCacheEntry:
    """
    Strongly typed representation of a framework cache entry for inspection.
    """

    url: Optional[str]
    homepage: Optional[str]
    platforms: List[str]
    resolved_at: Optional[str]
    expires_at: Optional[str]


@dataclass
class ResolvedUrlsCache:
    """
    Strongly typed representation of the complete resolved URLs cache.
    """

    platforms: Dict[str, PlatformCacheEntry] = field(default_factory=dict)  # type: ignore
    frameworks: Dict[str, FrameworkCacheEntry] = field(default_factory=dict)  # type: ignore


@dataclass
class PlatformResolution:
    """
    Cached resolution of a platform shorthand name to its metadata.
    """

    name: str
    repository_url: Optional[str] = None  # Kept for backward compatibility
    packages_url: Optional[str] = None  # Kept for backward compatibility
    version: Optional[str] = None
    frameworks: List[str] = field(default_factory=lambda: [])
    resolved_at: Optional[datetime] = None
    ttl_hours: int = 24

    # Enhanced multi-value fields
    enhanced_resolution: Optional[PlatformUrlResolution] = None

    @property
    def git_url(self) -> Optional[str]:
        """Get git URL from enhanced resolution or fall back to repository_url."""
        if self.enhanced_resolution:
            return self.enhanced_resolution.git_url
        return (
            self.repository_url
            if self.repository_url and self.repository_url.endswith(".git")
            else None
        )

    @property
    def zip_url(self) -> Optional[str]:
        """Get zip URL from enhanced resolution."""
        if self.enhanced_resolution:
            return self.enhanced_resolution.zip_url
        return (
            self.repository_url
            if self.repository_url and self.repository_url.endswith(".zip")
            else None
        )


@dataclass
class FrameworkResolution:
    """
    Cached resolution of a framework shorthand name to its metadata.
    """

    name: str
    url: Optional[str] = None  # Kept for backward compatibility
    homepage: Optional[str] = None
    platforms: List[str] = field(default_factory=lambda: [])
    resolved_at: Optional[datetime] = None
    ttl_hours: int = 24

    # Enhanced multi-value fields
    enhanced_resolution: Optional[FrameworkUrlResolution] = None

    @property
    def git_url(self) -> Optional[str]:
        """Get git URL from enhanced resolution."""
        if self.enhanced_resolution:
            return self.enhanced_resolution.git_url
        return self.url if self.url and self.url.endswith(".git") else None

    @property
    def zip_url(self) -> Optional[str]:
        """Get zip URL from enhanced resolution."""
        if self.enhanced_resolution:
            return self.enhanced_resolution.zip_url
        return self.url if self.url and self.url.endswith(".zip") else None


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

    def _is_url(self, value: str) -> bool:
        """Check if a value is already a URL (not a shorthand name)."""
        if not value:
            return False
        return any(
            value.startswith(scheme) for scheme in ("http://", "https://", "file://")
        )

    def _is_builtin_framework(self, framework_name: str) -> bool:
        """Check if a framework is a built-in PlatformIO framework that should not be resolved to URLs."""
        if not framework_name:
            return False
        # List of built-in PlatformIO frameworks that should remain as names, not URLs
        builtin_frameworks = {
            "arduino",
            "espidf",
            "cmsis",
            "libopencm3",
            "mbed",
            "freertos",
            "simba",
            "wiringpi",
            "pumbaa",
            "energia",
            "spl",
            "stm32cube",
            "zephyr",
            "framework-arduinoespressif32",
            "framework-espidf",
        }
        return framework_name.lower() in builtin_frameworks

    def _is_git_url(self, url: str) -> bool:
        """Check if a URL is a git repository URL."""
        if not url:
            return False
        # Git URLs typically end with .git or start with git://
        return url.endswith(".git") or url.startswith("git://")

    def _is_zip_url(self, url: str) -> bool:
        """Check if a URL points to a zip file."""
        if not url:
            return False
        return url.endswith(".zip") or ".zip?" in url

    def _classify_url_type(self, url: str) -> str:
        """Classify a URL as 'git', 'zip', 'file', or 'unknown'."""
        if not url:
            return "unknown"
        if self._is_zip_url(url):
            return "zip"
        elif self._is_git_url(url):
            return "git"
        elif url.startswith("file://") or (
            not self._is_url(url) and ("/" in url or "\\" in url)
        ):
            return "file"
        else:
            return "unknown"

    def _extract_packages_from_platform_data(
        self, platform_data: Dict[str, Any]
    ) -> List[PackageInfo]:
        """Extract package information from PlatformIO platform data (legacy method)."""
        packages: List[PackageInfo] = []
        packages_data = platform_data.get("packages", [])

        for pkg_data in packages_data:
            if not isinstance(pkg_data, dict):
                continue

            package = PackageInfo(
                name=str(pkg_data.get("name", "")),  # type: ignore
                type=str(pkg_data.get("type", "")),  # type: ignore
                requirements=str(pkg_data.get("requirements", "")),  # type: ignore
                url=str(pkg_data.get("url", "")),  # type: ignore
                optional=bool(pkg_data.get("optional", True)),  # type: ignore
                version=pkg_data.get("version"),  # type: ignore
                description=pkg_data.get("description"),  # type: ignore
            )
            packages.append(package)

        return packages

    def _extract_packages_from_platform_response(
        self, platform_show: PlatformShowResponse
    ) -> List[PackageInfo]:
        """Extract package information from typed PlatformShowResponse."""
        packages: List[PackageInfo] = []

        for pkg_data in platform_show.packages:
            if not isinstance(pkg_data, dict):
                continue

            package = PackageInfo(
                name=str(pkg_data.get("name", "")),  # type: ignore
                type=str(pkg_data.get("type", "")),  # type: ignore
                requirements=str(pkg_data.get("requirements", "")),  # type: ignore
                url=str(pkg_data.get("url", "")),  # type: ignore
                optional=bool(pkg_data.get("optional", True)),  # type: ignore
                version=pkg_data.get("version"),  # type: ignore
                description=pkg_data.get("description"),  # type: ignore
            )
            packages.append(package)

        return packages

    def _is_platform_cached(self, platform_name: str) -> bool:
        """Check if platform resolution is cached and still valid."""
        if not hasattr(self, "_platform_cache"):
            self._platform_cache: Dict[str, PlatformResolution] = {}

        if platform_name not in self._platform_cache:
            return False

        resolution = self._platform_cache[platform_name]
        if not resolution.resolved_at:
            return False

        age = datetime.now() - resolution.resolved_at
        return age < timedelta(hours=resolution.ttl_hours)

    def _is_framework_cached(self, framework_name: str) -> bool:
        """Check if framework resolution is cached and still valid."""
        if not hasattr(self, "_framework_cache"):
            self._framework_cache: Dict[str, FrameworkResolution] = {}

        if framework_name not in self._framework_cache:
            return False

        resolution = self._framework_cache[framework_name]
        if not resolution.resolved_at:
            return False

        age = datetime.now() - resolution.resolved_at
        return age < timedelta(hours=resolution.ttl_hours)

    def _run_pio_command(
        self, args: List[str]
    ) -> Optional[Union[Dict[str, Any], List[Dict[str, Any]]]]:
        """Run a PlatformIO CLI command and return JSON output."""
        try:
            cmd = ["pio"] + args
            logger.debug(f"Running PlatformIO command: {' '.join(cmd)}")

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30,
                check=True,
            )

            if result.stdout:
                try:
                    parsed_output = json.loads(result.stdout)
                    # Return parsed output if it's a dict or list
                    if isinstance(parsed_output, dict):
                        return parsed_output  # type: ignore[reportUnknownVariableType]
                    elif isinstance(parsed_output, list):
                        return parsed_output  # type: ignore[reportUnknownVariableType]
                    else:
                        logger.warning(f"Unexpected JSON type: {type(parsed_output)}")
                        return None
                except json.JSONDecodeError as e:
                    logger.error(f"Failed to parse PlatformIO JSON output: {e}")
                    logger.debug(f"Raw output: {result.stdout[:500]}")
                    return None

            return None

        except subprocess.TimeoutExpired:
            cmd_str = " ".join(["pio"] + args)
            logger.error(f"PlatformIO command timed out: {cmd_str}")
        except subprocess.CalledProcessError as e:
            cmd_str = " ".join(["pio"] + args)
            logger.error(f"PlatformIO command failed: {e}")
            logger.debug(f"Command output: {e.stdout}, Error: {e.stderr}")
        except FileNotFoundError:
            logger.error("PlatformIO CLI not found. Is it installed and in PATH?")
        except Exception as e:
            cmd_str = " ".join(["pio"] + args)
            logger.error(f"Unexpected error running PlatformIO command: {e}")

        return None

    def _get_platform_show_typed(
        self, platform_name: str
    ) -> Optional[PlatformShowResponse]:
        """Get typed platform information from PlatformIO CLI."""
        raw_data = self._run_pio_command(
            ["platform", "show", platform_name, "--json-output"]
        )
        if raw_data and isinstance(raw_data, dict):
            try:
                return PlatformShowResponse.from_dict(raw_data)  # type: ignore
            except Exception as e:
                logger.error(
                    f"Failed to parse platform show response for {platform_name}: {e}"
                )
                return None
        return None

    def _get_frameworks_list_typed(self) -> List[FrameworkInfo]:
        """Get typed list of available frameworks from PlatformIO CLI."""
        raw_data = self._run_pio_command(["platform", "frameworks", "--json-output"])
        if not raw_data:
            return []

        frameworks_list: List[FrameworkInfo] = []
        try:
            # Handle both dict and list responses
            if isinstance(raw_data, dict):
                frameworks_data = [raw_data]
            elif isinstance(raw_data, list):
                frameworks_data = raw_data
            else:
                logger.warning("Unexpected frameworks data type from PlatformIO")
                return []

            for fw_data in frameworks_data:
                if isinstance(fw_data, dict):
                    try:
                        framework = FrameworkInfo.from_dict(fw_data)
                        frameworks_list.append(framework)
                    except Exception as e:
                        logger.warning(f"Failed to parse framework data: {e}")
                        continue

        except Exception as e:
            logger.error(f"Failed to parse frameworks list response: {e}")
            return []

        return frameworks_list

    def resolve_platform_url(self, platform_name: str) -> Optional[str]:
        """
        Resolve a platform shorthand name to its repository URL.

        Args:
            platform_name: Platform name like 'espressif32', 'atmelavr', etc.

        Returns:
            The repository URL for the platform, or None if resolution fails.
        """
        if self._is_url(platform_name):
            return platform_name

        # Check cache first
        if self._is_platform_cached(platform_name):
            cached = self._platform_cache[platform_name]
            logger.debug(f"Using cached platform resolution for {platform_name}")
            return cached.repository_url

        # Resolve via PlatformIO CLI
        platform_data = self._run_pio_command(
            ["platform", "show", platform_name, "--json-output"]
        )

        if not platform_data or not isinstance(platform_data, dict):
            logger.warning(f"Failed to resolve platform: {platform_name}")
            return None

        # Extract repository URL from platform data
        repository_url = platform_data.get("repository")
        version = platform_data.get("version")
        frameworks = platform_data.get("frameworks", [])

        # Cache the resolution
        if not hasattr(self, "_platform_cache"):
            self._platform_cache: Dict[str, PlatformResolution] = {}

        self._platform_cache[platform_name] = PlatformResolution(
            name=platform_name,
            repository_url=repository_url,
            version=version,
            frameworks=frameworks,
            resolved_at=datetime.now(),
        )

        logger.debug(f"Resolved platform {platform_name} -> {repository_url}")
        return repository_url

    def resolve_platform_url_enhanced(
        self, platform_name: str
    ) -> Optional[PlatformUrlResolution]:
        """
        Resolve a platform shorthand name to comprehensive URL information.

        Args:
            platform_name: Platform name like 'espressif32', 'atmelavr', etc.

        Returns:
            PlatformUrlResolution with git_url, zip_url, packages, etc. or None if resolution fails.
        """
        # Check if it's already a URL or local path
        url_type = self._classify_url_type(platform_name)
        if url_type in ("git", "zip", "file"):
            resolution = PlatformUrlResolution(name=platform_name)

            if url_type == "git":
                resolution.git_url = platform_name
            elif url_type == "zip":
                resolution.zip_url = platform_name
            elif url_type == "file":
                resolution.local_path = platform_name

            return resolution

        # Check cache first
        if self._is_platform_cached(platform_name):
            cached = self._platform_cache[platform_name]
            logger.debug(
                f"Using cached enhanced platform resolution for {platform_name}"
            )
            if cached.enhanced_resolution:
                return cached.enhanced_resolution
            # Fall back to creating resolution from cached data
            return PlatformUrlResolution(
                name=platform_name,
                git_url=cached.git_url,
                zip_url=cached.zip_url,
                version=cached.version,
                frameworks=cached.frameworks,
            )

        # Resolve via PlatformIO CLI using typed response
        platform_show = self._get_platform_show_typed(platform_name)
        if not platform_show:
            logger.warning(f"Failed to resolve platform: {platform_name}")
            return None

        # Extract package information from typed response
        packages = self._extract_packages_from_platform_response(platform_show)

        # Classify the repository URL type
        git_url = None
        zip_url = None

        if platform_show.repository:
            url_type = self._classify_url_type(platform_show.repository)
            if url_type == "git":
                git_url = platform_show.repository
            elif url_type == "zip":
                zip_url = platform_show.repository

        # Look for additional zip URLs in packages (some platforms have main zip URLs in packages)
        for package in packages:
            if package.requirements and self._is_zip_url(package.requirements):
                # Prefer platform-level zip URLs over individual package URLs
                if not zip_url and package.type in ("", "framework"):
                    zip_url = package.requirements

        resolution = PlatformUrlResolution(
            name=platform_name,
            git_url=git_url,
            zip_url=zip_url,
            version=platform_show.version,
            frameworks=platform_show.frameworks,
            packages=packages,
            homepage=platform_show.homepage,
        )

        # Cache the enhanced resolution
        if not hasattr(self, "_platform_cache"):
            self._platform_cache: Dict[str, PlatformResolution] = {}

        cached_resolution = PlatformResolution(
            name=platform_name,
            repository_url=git_url or zip_url,  # For backward compatibility
            version=platform_show.version,
            frameworks=platform_show.frameworks,
            resolved_at=datetime.now(),
            enhanced_resolution=resolution,
        )
        self._platform_cache[platform_name] = cached_resolution

        logger.debug(
            f"Enhanced resolution for {platform_name}: git={git_url}, zip={zip_url}, packages={len(packages)}"
        )
        return resolution

    def resolve_framework_url(self, framework_name: str) -> Optional[str]:
        """
        Resolve a framework shorthand name to its URL.

        Args:
            framework_name: Framework name like 'arduino', 'espidf', etc.

        Returns:
            The URL for the framework, or None if resolution fails.
        """
        if self._is_url(framework_name):
            return framework_name

        # Special case for espidf - it's not in the global frameworks list but is commonly used
        if framework_name == "espidf":
            return "https://github.com/espressif/esp-idf"

        # Check cache first
        if self._is_framework_cached(framework_name):
            cached = self._framework_cache[framework_name]
            logger.debug(f"Using cached framework resolution for {framework_name}")
            return cached.url

        # Resolve via PlatformIO CLI using typed response
        frameworks_list = self._get_frameworks_list_typed()

        if not frameworks_list:
            logger.warning("Failed to get frameworks list from PlatformIO")
            return None

        # Find the specific framework
        framework_info: Optional[FrameworkInfo] = None
        for fw in frameworks_list:
            if fw.name == framework_name:
                framework_info = fw
                break

        if not framework_info:
            logger.warning(f"Framework not found: {framework_name}")
            return None

        # Extract URL from framework data
        url = framework_info.url or framework_info.homepage
        homepage = framework_info.homepage
        platforms = framework_info.platforms

        # Cache the resolution
        if not hasattr(self, "_framework_cache"):
            self._framework_cache: Dict[str, FrameworkResolution] = {}

        self._framework_cache[framework_name] = FrameworkResolution(
            name=framework_name,
            url=url,
            homepage=homepage,
            platforms=platforms,
            resolved_at=datetime.now(),
        )

        logger.debug(f"Resolved framework {framework_name} -> {url}")
        return url

    def resolve_framework_url_enhanced(
        self, framework_name: str
    ) -> Optional[FrameworkUrlResolution]:
        """
        Resolve a framework shorthand name to comprehensive URL information.

        Args:
            framework_name: Framework name like 'arduino', 'espidf', etc.

        Returns:
            FrameworkUrlResolution with git_url, zip_url, homepage, etc. or None if resolution fails.
        """
        if self._is_url(framework_name):
            # If it's already a URL, create a resolution based on URL type
            url_type = self._classify_url_type(framework_name)
            resolution = FrameworkUrlResolution(name=framework_name)

            if url_type == "git":
                resolution.git_url = framework_name
            elif url_type == "zip":
                resolution.zip_url = framework_name

            return resolution

        # Check cache first
        if self._is_framework_cached(framework_name):
            cached = self._framework_cache[framework_name]
            logger.debug(
                f"Using cached enhanced framework resolution for {framework_name}"
            )
            if cached.enhanced_resolution:
                return cached.enhanced_resolution
            # Fall back to creating resolution from cached data
            return FrameworkUrlResolution(
                name=framework_name,
                git_url=cached.git_url,
                zip_url=cached.zip_url,
                homepage=cached.homepage,
                platforms=cached.platforms,
                url=cached.url,  # Include the original URL
            )

        # Resolve via PlatformIO CLI using typed response
        frameworks_list = self._get_frameworks_list_typed()

        if not frameworks_list:
            logger.warning("Failed to get frameworks list from PlatformIO")
            return None

        # Find the specific framework
        framework_info: Optional[FrameworkInfo] = None
        for fw in frameworks_list:
            if fw.name == framework_name:
                framework_info = fw
                break

        if not framework_info:
            logger.warning(f"Framework not found: {framework_name}")
            return None

        # Classify URLs by type
        git_url = None
        zip_url = None

        # Check the main URL
        if framework_info.url:
            url_type = self._classify_url_type(framework_info.url)
            if url_type == "git":
                git_url = framework_info.url
            elif url_type == "zip":
                zip_url = framework_info.url

        # Check homepage URL as potential git repository
        if framework_info.homepage and not git_url:
            homepage_type = self._classify_url_type(framework_info.homepage)
            if homepage_type == "git":
                git_url = framework_info.homepage

        resolution = FrameworkUrlResolution(
            name=framework_name,
            git_url=git_url,
            zip_url=zip_url,
            homepage=framework_info.homepage,
            platforms=framework_info.platforms,
            version=framework_info.version,
            title=framework_info.title,
            description=framework_info.description,
            url=framework_info.url,  # Include the original URL from CLI response
        )

        # Cache the enhanced resolution
        if not hasattr(self, "_framework_cache"):
            self._framework_cache: Dict[str, FrameworkResolution] = {}

        cached_resolution = FrameworkResolution(
            name=framework_name,
            url=git_url or zip_url or framework_info.url,  # For backward compatibility
            homepage=framework_info.homepage,
            platforms=framework_info.platforms,
            resolved_at=datetime.now(),
            enhanced_resolution=resolution,
        )
        self._framework_cache[framework_name] = cached_resolution

        logger.debug(
            f"Enhanced resolution for framework {framework_name}: git={git_url}, zip={zip_url}, homepage={framework_info.homepage}"
        )
        return resolution

    def resolve_platform_urls(self) -> Dict[str, Optional[str]]:
        """
        Resolve all platform shorthand names in the configuration to URLs.

        Returns:
            Dictionary mapping platform names to their resolved URLs.
        """
        resolutions: Dict[str, Optional[str]] = {}

        for section_name, option_name, platform_value in self.get_platform_urls():
            if platform_value and not self._is_url(platform_value):
                if platform_value not in resolutions:
                    resolutions[platform_value] = self.resolve_platform_url(
                        platform_value
                    )

        return resolutions

    def resolve_framework_urls(self) -> Dict[str, Optional[str]]:
        """
        Resolve all framework shorthand names in the configuration to URLs.

        Returns:
            Dictionary mapping framework names to their resolved URLs.
        """
        resolutions: Dict[str, Optional[str]] = {}

        for section_name, option_name, framework_value in self.get_framework_urls():
            if (
                framework_value
                and not self._is_url(framework_value)
                and not self._is_builtin_framework(framework_value)
            ):
                if framework_value not in resolutions:
                    resolutions[framework_value] = self.resolve_framework_url(
                        framework_value
                    )

        return resolutions

    def get_resolved_urls_cache(self) -> ResolvedUrlsCache:
        """
        Get cached resolution results for inspection.

        Returns:
            Strongly typed cache data with platform and framework resolutions.
        """
        result = ResolvedUrlsCache()

        if hasattr(self, "_platform_cache"):
            for name, resolution in self._platform_cache.items():
                cache_entry = PlatformCacheEntry(
                    repository_url=resolution.repository_url,
                    version=resolution.version,
                    frameworks=resolution.frameworks,
                    resolved_at=resolution.resolved_at.isoformat()
                    if resolution.resolved_at
                    else None,
                    expires_at=(
                        resolution.resolved_at + timedelta(hours=resolution.ttl_hours)
                    ).isoformat()
                    if resolution.resolved_at
                    else None,
                )
                result.platforms[name] = cache_entry

        if hasattr(self, "_framework_cache"):
            for name, resolution in self._framework_cache.items():
                cache_entry = FrameworkCacheEntry(
                    url=resolution.url,
                    homepage=resolution.homepage,
                    platforms=resolution.platforms,
                    resolved_at=resolution.resolved_at.isoformat()
                    if resolution.resolved_at
                    else None,
                    expires_at=(
                        resolution.resolved_at + timedelta(hours=resolution.ttl_hours)
                    ).isoformat()
                    if resolution.resolved_at
                    else None,
                )
                result.frameworks[name] = cache_entry

        return result

    def invalidate_resolution_cache(self) -> None:
        """Clear cached URL resolutions."""
        if hasattr(self, "_platform_cache"):
            self._platform_cache.clear()
        if hasattr(self, "_framework_cache"):
            self._framework_cache.clear()
        logger.debug("Cleared URL resolution cache")

    def __str__(self) -> str:
        """Return string representation of the configuration."""
        output = io.StringIO()
        self.config.write(output)
        return output.getvalue()

    def optimize(self, cache: "PlatformIOCache", full: bool = False) -> None:
        """
        Download all packages and swap URLs for local file path URLs.

        This method will:
        1. Resolve platform shorthand names to URLs using PlatformIO CLI (only for *.zip URLs when full=False)
        2. Replace shorthand names with resolved URLs in-place (only when full=True for frameworks)
        3. Find all platform and framework URLs that point to zip files
        4. Download and cache them using the provided PlatformIO cache system
        5. Replace the URLs in-place with local file:// URLs

        Args:
            cache: PlatformIOCache instance to use for downloading and caching packages.
            full: When True, resolve all platform/framework shorthand names to URLs.
                 When False (default), only optimize *.zip URLs for caching.
        """
        # Import here to avoid circular dependencies
        from ci.compiler.platformio_cache import _is_zip_web_url, handle_zip_artifact

        cache_manager = cache

        # Step 1: Resolve shorthand platform names to URLs (always happens)
        logger.debug("Resolving platform shorthand names to URLs...")
        platform_resolutions = self.resolve_platform_urls()
        platform_replacements_made = 0

        for platform_name, resolved_url in platform_resolutions.items():
            if resolved_url:
                # Replace shorthand names with resolved URLs (regardless of whether they're zip URLs)
                for (
                    section_name,
                    option_name,
                    current_url,
                ) in self.get_platform_urls():
                    if current_url == platform_name:
                        self.replace_url(
                            section_name, option_name, platform_name, resolved_url
                        )
                        platform_replacements_made += 1
                        logger.debug(
                            f"Resolved platform {platform_name} -> {resolved_url} in {section_name}"
                        )

        if platform_replacements_made > 0:
            logger.info(
                f"Resolved {platform_replacements_made} platform shorthand names"
            )

        # Step 2: Resolve shorthand framework names to URLs (always happens)
        logger.debug("Resolving framework shorthand names to URLs...")
        framework_resolutions = self.resolve_framework_urls()
        framework_replacements_made = 0

        for framework_name, resolved_url in framework_resolutions.items():
            if resolved_url:
                # Replace shorthand names with resolved URLs (regardless of whether they're zip URLs)
                for (
                    section_name,
                    option_name,
                    current_url,
                ) in self.get_framework_urls():
                    if current_url == framework_name:
                        self.replace_url(
                            section_name, option_name, framework_name, resolved_url
                        )
                        framework_replacements_made += 1
                        logger.debug(
                            f"Resolved framework {framework_name} -> {resolved_url} in {section_name}"
                        )

        if framework_replacements_made > 0:
            logger.info(
                f"Resolved {framework_replacements_made} framework shorthand names"
            )

        # Step 3: Find all platform and framework URLs that need optimization (*.zip caching always happens)
        zip_artifacts: List[Tuple[str, bool, str]] = []

        # Scan platform URLs
        for section_name, option_name, url in self.get_platform_urls():
            if _is_zip_web_url(url):
                zip_artifacts.append((url, False, section_name))  # False = platform

        # Scan framework URLs
        for section_name, option_name, url in self.get_framework_urls():
            if _is_zip_web_url(url):
                zip_artifacts.append((url, True, section_name))  # True = framework

        if not zip_artifacts:
            logger.debug("No zip artifacts found to cache after URL resolution")
            return

        # Step 4: Deduplicate artifacts by URL to avoid redundant processing
        unique_artifacts: Dict[
            str, Tuple[bool, str]
        ] = {}  # url -> (is_framework, env_section)
        for artifact_url, is_framework, env_section in zip_artifacts:
            if artifact_url not in unique_artifacts:
                unique_artifacts[artifact_url] = (is_framework, env_section)

        logger.info(
            f"Processing {len(unique_artifacts)} unique zip artifacts for caching"
        )

        # Step 5: Process each unique artifact and collect URL replacements
        replacements: Dict[str, str] = {}
        for artifact_url, (is_framework, env_section) in unique_artifacts.items():
            resolved_path = handle_zip_artifact(
                artifact_url, cache_manager, env_section
            )
            replacements[artifact_url] = resolved_path

        # Step 6: Apply replacements to the configuration
        if replacements:
            # Replace platform URLs
            for section_name, option_name, url in self.get_platform_urls():
                if url in replacements:
                    self.replace_url(section_name, option_name, url, replacements[url])

            # Replace framework URLs
            for section_name, option_name, url in self.get_framework_urls():
                if url in replacements:
                    self.replace_url(section_name, option_name, url, replacements[url])

            logger.info(f"Replaced {len(replacements)} URLs with cached local paths")

    def optimize_enhanced(self, cache: "PlatformIOCache") -> None:
        """
        Enhanced optimization using multi-value URL resolution.

        This method leverages the enhanced resolution system to:
        1. Resolve shorthand names to multiple URL types (git, zip)
        2. Prefer zip URLs for faster caching when available
        3. Fall back to git URLs when zip URLs are not available
        4. Update local_path in resolution results after caching

        Args:
            cache: PlatformIOCache instance to use for downloading and caching packages.
        """
        # Import here to avoid circular dependencies
        from ci.compiler.platformio_cache import _is_zip_web_url, handle_zip_artifact

        cache_manager = cache

        # Step 1: Resolve shorthand platform names using enhanced resolution
        logger.debug("Resolving platform shorthand names using enhanced resolution...")
        platform_replacements_made = 0
        platform_resolutions: Dict[str, PlatformUrlResolution] = {}

        for section_name, option_name, platform_value in self.get_platform_urls():
            if platform_value and not self._is_url(platform_value):
                if platform_value not in platform_resolutions:
                    resolution = self.resolve_platform_url_enhanced(platform_value)
                    if resolution:
                        platform_resolutions[platform_value] = resolution

        # Replace shorthand names with preferred URLs and track for caching
        for platform_name, resolution in platform_resolutions.items():
            preferred_url = resolution.preferred_url
            if preferred_url:
                for section_name, option_name, current_url in self.get_platform_urls():
                    if current_url == platform_name:
                        self.replace_url(
                            section_name, option_name, platform_name, preferred_url
                        )
                        platform_replacements_made += 1
                        logger.debug(
                            f"Enhanced platform resolution {platform_name} -> {preferred_url} in {section_name}"
                        )

        if platform_replacements_made > 0:
            logger.info(
                f"Enhanced resolution of {platform_replacements_made} platform shorthand names"
            )

        # Step 2: Resolve shorthand framework names using enhanced resolution
        logger.debug("Resolving framework shorthand names using enhanced resolution...")
        framework_replacements_made = 0
        framework_resolutions: Dict[str, FrameworkUrlResolution] = {}

        for section_name, option_name, framework_value in self.get_framework_urls():
            if (
                framework_value
                and not self._is_url(framework_value)
                and not self._is_builtin_framework(framework_value)
            ):
                if framework_value not in framework_resolutions:
                    resolution = self.resolve_framework_url_enhanced(framework_value)
                    if resolution:
                        framework_resolutions[framework_value] = resolution

        # Replace shorthand names with preferred URLs
        for framework_name, resolution in framework_resolutions.items():
            preferred_url = resolution.preferred_url
            if preferred_url:
                for section_name, option_name, current_url in self.get_framework_urls():
                    if current_url == framework_name:
                        self.replace_url(
                            section_name, option_name, framework_name, preferred_url
                        )
                        framework_replacements_made += 1
                        logger.debug(
                            f"Enhanced framework resolution {framework_name} -> {preferred_url} in {section_name}"
                        )

        if framework_replacements_made > 0:
            logger.info(
                f"Enhanced resolution of {framework_replacements_made} framework shorthand names"
            )

        # Step 3: Cache downloadable URLs and update local paths
        zip_artifacts: List[Tuple[str, bool, str]] = []

        # Scan for zip URLs that can be cached
        for section_name, option_name, url in self.get_platform_urls():
            if _is_zip_web_url(url):
                zip_artifacts.append((url, False, section_name))

        for section_name, option_name, url in self.get_framework_urls():
            if _is_zip_web_url(url):
                zip_artifacts.append((url, True, section_name))

        if not zip_artifacts:
            logger.debug("No zip URLs found to cache after enhanced resolution")
            return

        # Process caching as before
        unique_artifacts: Dict[str, Tuple[bool, str]] = {}
        for artifact_url, is_framework, env_section in zip_artifacts:
            if artifact_url not in unique_artifacts:
                unique_artifacts[artifact_url] = (is_framework, env_section)

        logger.info(
            f"Processing {len(unique_artifacts)} unique zip artifacts for enhanced caching"
        )

        # Cache artifacts and collect replacements
        replacements: Dict[str, str] = {}
        for artifact_url, (is_framework, env_section) in unique_artifacts.items():
            resolved_path = handle_zip_artifact(
                artifact_url, cache_manager, env_section
            )
            replacements[artifact_url] = resolved_path

        # Apply replacements and update resolution local_path fields
        if replacements:
            # Update platform resolutions with local paths
            for platform_name, resolution in platform_resolutions.items():
                if resolution.zip_url in replacements:
                    resolution.local_path = replacements[resolution.zip_url]
                elif resolution.git_url in replacements:
                    resolution.local_path = replacements[resolution.git_url]

            # Update framework resolutions with local paths
            for framework_name, resolution in framework_resolutions.items():
                if resolution.zip_url in replacements:
                    resolution.local_path = replacements[resolution.zip_url]
                elif resolution.git_url in replacements:
                    resolution.local_path = replacements[resolution.git_url]

            # Replace URLs in configuration
            for section_name, option_name, url in self.get_platform_urls():
                if url in replacements:
                    self.replace_url(section_name, option_name, url, replacements[url])

            for section_name, option_name, url in self.get_framework_urls():
                if url in replacements:
                    self.replace_url(section_name, option_name, url, replacements[url])

            logger.info(
                f"Enhanced caching: Replaced {len(replacements)} URLs with cached local paths"
            )
