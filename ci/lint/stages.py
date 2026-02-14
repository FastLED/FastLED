"""Lint stage definitions."""

from dataclasses import dataclass, field
from typing import Callable, Optional


@dataclass
class LintStage:
    """
    Represents a single linting stage.

    Attributes:
        name: Internal name (e.g., "cpp_linting", "ruff")
        display_name: Human-readable name (e.g., "C++ LINTING", "RUFF")
        run_fn: Function to execute the stage (returns True on success)
        check_cache_fn: Optional function to check fingerprint cache
        mark_success_fn: Optional function to mark cache success
        dependencies: List of stage names that must complete first
        timeout: Timeout in seconds
    """

    name: str
    display_name: str
    run_fn: Callable[[], bool]
    check_cache_fn: Optional[Callable[[], bool]] = None
    mark_success_fn: Optional[Callable[[], None]] = None
    dependencies: list[str] = field(default_factory=lambda: [])
    timeout: float = 300.0
