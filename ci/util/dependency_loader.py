#!/usr/bin/env python3
"""
Load and manage dependencies from centralized dependencies.json manifest.

This provides a single source of truth for all file patterns monitored
by CI/linting/testing operations.
"""

import json
from pathlib import Path
from typing import Any, Optional


class DependencyManifest:
    """Load and query the centralized dependencies manifest."""

    def __init__(self, manifest_path: Optional[Path] = None):
        """
        Initialize the dependency manifest.

        Args:
            manifest_path: Path to dependencies.json (defaults to ci/dependencies.json)
        """
        if manifest_path is None:
            # Try multiple locations in order of preference
            candidates = [
                Path("ci") / "dependencies.json",  # Primary location
                Path(".") / "dependencies.json",  # Fallback for backward compatibility
            ]

            manifest_path = None
            for candidate in candidates:
                if candidate.exists():
                    manifest_path = candidate
                    break

            if manifest_path is None:
                raise FileNotFoundError(
                    f"dependencies.json not found in any of: {', '.join(str(c) for c in candidates)}"
                )

        if not manifest_path.exists():
            raise FileNotFoundError(f"dependencies.json not found at {manifest_path}")

        self.manifest_path = manifest_path
        with open(manifest_path, "r") as f:
            self.data = json.load(f)

    def get_globs(self, operation: str) -> list[str]:
        """
        Get file patterns (globs) for a given operation.

        Args:
            operation: Operation name (e.g., "python_lint", "cpp_lint")

        Returns:
            List of glob patterns

        Raises:
            KeyError: If operation not found in manifest
        """
        if operation not in self.data["operations"]:
            raise KeyError(
                f"Operation '{operation}' not found in manifest. Available: "
                f"{', '.join(self.data['operations'].keys())}"
            )

        return self.data["operations"][operation].get("globs", [])

    def get_excludes(self, operation: str) -> list[str]:
        """
        Get exclusion patterns for a given operation.

        Args:
            operation: Operation name

        Returns:
            List of exclusion patterns (or empty list if none)
        """
        if operation not in self.data["operations"]:
            return []

        return self.data["operations"][operation].get("excludes", [])

    def get_operation(self, operation: str) -> dict[str, Any]:
        """
        Get full operation definition.

        Args:
            operation: Operation name

        Returns:
            Full operation dictionary

        Raises:
            KeyError: If operation not found
        """
        if operation not in self.data["operations"]:
            raise KeyError(f"Operation '{operation}' not found in manifest")

        return self.data["operations"][operation]

    def get_cache_key(self, operation: str) -> str:
        """Get cache key for an operation."""
        return self.get_operation(operation).get("cache_key", operation)

    def get_tools(self, operation: str) -> list[str]:
        """Get list of tools used by an operation."""
        return self.get_operation(operation).get("tools", [])

    def list_operations(self) -> list[str]:
        """Get list of all defined operations."""
        return list(self.data["operations"].keys())

    def get_description(self, operation: str) -> str:
        """Get human-readable description of operation."""
        return self.get_operation(operation).get("description", "")

    def print_summary(self) -> None:
        """Print a summary of all operations and their dependencies."""
        print("\n" + "=" * 70)
        print("FASTLED DEPENDENCY MANIFEST SUMMARY")
        print("=" * 70)

        for op_name in self.list_operations():
            op = self.get_operation(op_name)
            globs = op.get("globs", [])
            print(f"\n📋 {op_name}")
            print(f"   {op.get('description', 'No description')}")
            print(f"   Patterns: {len(globs)} glob(s)")
            for glob in globs[:3]:  # Show first 3
                print(f"     - {glob}")
            if len(globs) > 3:
                print(f"     ... and {len(globs) - 3} more")


def load_globs_for_operation(operation: str) -> list[str]:
    """
    Convenience function: Load globs for an operation.

    Args:
        operation: Operation name

    Returns:
        List of glob patterns
    """
    manifest = DependencyManifest()
    return manifest.get_globs(operation)


def load_operation_config(operation: str) -> dict[str, Any]:
    """
    Convenience function: Load full operation config.

    Args:
        operation: Operation name

    Returns:
        Full operation configuration dictionary
    """
    manifest = DependencyManifest()
    return manifest.get_operation(operation)


if __name__ == "__main__":
    import sys

    try:
        manifest = DependencyManifest()

        if len(sys.argv) > 1:
            operation = sys.argv[1]
            try:
                globs = manifest.get_globs(operation)
                print(f"Globs for '{operation}':")
                for glob in globs:
                    print(f"  {glob}")
            except KeyError as e:
                print(f"Error: {e}")
                sys.exit(1)
        else:
            manifest.print_summary()

    except FileNotFoundError as e:
        print(f"Error: {e}")
        sys.exit(1)
