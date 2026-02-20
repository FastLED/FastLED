"""Meson build system integration for FastLED.

This package provides modular tools for building and testing FastLED using Meson.
Refactored from monolithic ci/util/meson_runner.py into focused modules.

Module Structure:
- output.py: Output formatting and colored printing
- io_utils.py: Safe file I/O operations
- compiler.py: Compiler detection and Meson executable resolution
- test_discovery.py: Test discovery and fuzzy matching
- build_config.py: Build directory setup and configuration
- compile.py: Compilation execution
- test_execution.py: Test execution via meson test
- streaming.py: Parallel streaming compilation and testing
- runner.py: Main orchestration and CLI entry point

Performance note: This __init__.py uses lazy __getattr__ loading instead of
eager imports to avoid loading the entire meson runner chain (~400ms) whenever
any ci.meson submodule is first imported. Previously, importing ci.meson.compiler
(used by test_discovery.py) would trigger loading ci.meson.runner and its heavy
dependencies (clang_tool_chain, build_config, streaming, etc.) via this __init__.py.
With lazy loading, each submodule is only loaded when actually accessed via
'from ci.meson import SomeName' syntax.
"""

import importlib
from dataclasses import dataclass


__all__ = [
    "check_meson_installed",
    "check_meson_version_compatibility",
    "get_meson_executable",
    "get_meson_version",
    "run_meson_build_and_test",
    "MesonTestResult",
    "perform_ninja_maintenance",
    "setup_meson_build",
    "_print_banner",
]


@dataclass(frozen=True)
class _LazyAttr:
    """Specification for a lazily-loaded module attribute."""

    module_path: str
    attr_name: str


# Mapping from attribute name to lazy load specification
_LAZY_ATTRS: dict[str, _LazyAttr] = {
    "run_meson_build_and_test": _LazyAttr(
        "ci.meson.runner", "run_meson_build_and_test"
    ),
    "MesonTestResult": _LazyAttr("ci.meson.test_execution", "MesonTestResult"),
    "check_meson_installed": _LazyAttr("ci.meson.compiler", "check_meson_installed"),
    "check_meson_version_compatibility": _LazyAttr(
        "ci.meson.compiler",
        "check_meson_version_compatibility",
    ),
    "get_meson_executable": _LazyAttr("ci.meson.compiler", "get_meson_executable"),
    "get_meson_version": _LazyAttr("ci.meson.compiler", "get_meson_version"),
    "perform_ninja_maintenance": _LazyAttr(
        "ci.meson.build_config",
        "perform_ninja_maintenance",
    ),
    "setup_meson_build": _LazyAttr("ci.meson.build_config", "setup_meson_build"),
    "_print_banner": _LazyAttr("ci.meson.output", "_print_banner"),
}


def __getattr__(name: str) -> object:
    """Lazy-load package-level attributes on first access.

    This avoids eagerly importing ci.meson.runner (and its heavy transitive
    dependencies like clang_tool_chain) when only lightweight submodules such
    as ci.meson.compiler or ci.meson.test_discovery are needed.
    """
    if name in _LAZY_ATTRS:
        lazy_attr = _LAZY_ATTRS[name]
        module = importlib.import_module(lazy_attr.module_path)
        obj = getattr(module, lazy_attr.attr_name)
        # Cache in globals() so subsequent access avoids __getattr__ overhead
        globals()[name] = obj
        return obj
    raise AttributeError(f"module 'ci.meson' has no attribute {name!r}")
