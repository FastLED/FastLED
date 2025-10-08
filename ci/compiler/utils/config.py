"""Configuration and result data classes for example compilation testing."""

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional


@dataclass
class CompilationResult:
    """Results from compiling a set of examples."""

    successful_count: int
    failed_count: int
    compile_time: float
    failed_examples: List[Dict[str, Any]]
    object_file_map: Optional[Dict[Path, List[Path]]]


@dataclass
class LinkingResult:
    """Results from linking examples into executable programs."""

    linked_count: int
    failed_count: int
    cache_hits: int = 0
    cache_misses: int = 0

    @property
    def total_count(self) -> int:
        """Total number of examples processed (linked + failed)"""
        return self.linked_count + self.failed_count

    @property
    def cache_hit_rate(self) -> float:
        """Cache hit rate as percentage"""
        total_processed = self.cache_hits + self.cache_misses
        return (self.cache_hits / total_processed * 100) if total_processed > 0 else 0.0


@dataclass
class ExecutionFailure:
    """Represents a failed example execution with identifying info and reason."""

    name: str
    reason: str
    stdout: str


@dataclass
class CompilationTestConfig:
    """Configuration for the compilation test."""

    specific_examples: Optional[List[str]]
    clean_build: bool
    disable_pch: bool
    unity_build: bool
    unity_custom_output: Optional[str]
    unity_additional_flags: Optional[List[str]]
    full_compilation: bool
    no_parallel: bool
    verbose: bool


@dataclass
class CompilationTestResults:
    """Results from compilation test."""

    successful_count: int
    failed_count: int
    failed_examples: List[Dict[str, str]]
    compile_time: float
    linking_time: float
    linked_count: int
    linking_failed_count: int
    object_file_map: Optional[Dict[Path, List[Path]]]
    execution_failures: List[ExecutionFailure]
    # Sketch runner execution results
    executed_count: int = 0
    execution_failed_count: int = 0
    execution_time: float = 0.0
