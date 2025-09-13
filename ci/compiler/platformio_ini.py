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
import tempfile
import uuid
from pathlib import Path
from typing import TYPE_CHECKING, Any, Dict, List, Optional, Tuple, Union


if TYPE_CHECKING:
    from ci.compiler.platformio_cache import PlatformIOCache


# Configure logging
logger = logging.getLogger(__name__)


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

    def remove_option(self, section: str, option: str) -> bool:
        """
        Remove an option from a section.

        Args:
            section: Section name
            option: Option name

        Returns:
            True if option was removed, False if it didn't exist
        """
        return self.config.remove_option(section, option)

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
        for section_name, options in data.items():
            self.config.add_section(section_name)
            for option, value in options.items():
                self.config.set(section_name, option, value)

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
