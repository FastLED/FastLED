#!/usr/bin/env python3
from dataclasses import dataclass
from typing import List, Optional

from typeguard import typechecked


@typechecked
@dataclass
class TestArgs:
    """Type-safe test arguments"""

    cpp: bool = False
    unit: bool = False
    py: bool = False
    test: Optional[str] = None
    clang: bool = False
    gcc: bool = False
    clean: bool = False
    no_interactive: bool = False
    interactive: bool = False
    verbose: bool = False
    quick: bool = False
    no_stack_trace: bool = False
    check: bool = False
    examples: Optional[list[str]] = None
    no_pch: bool = False
    cache: bool = False
    unity: bool = False
    full: bool = False
    legacy: bool = False  # Use legacy CMake system instead of new Python API


@typechecked
@dataclass
class TestCategories:
    """Type-safe test category flags"""

    unit: bool
    examples: bool
    py: bool
    integration: bool
    unit_only: bool
    examples_only: bool
    py_only: bool
    integration_only: bool

    def __post_init__(self):
        # Type validation
        for field_name in [
            "unit",
            "examples",
            "py",
            "integration",
            "unit_only",
            "examples_only",
            "py_only",
            "integration_only",
        ]:
            value = getattr(self, field_name)
            if not isinstance(value, bool):
                raise TypeError(f"{field_name} must be bool, got {type(value)}")
