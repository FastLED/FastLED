#!/usr/bin/env python3
"""
Efficient compilation using pio run with persistent projects.
Implements the two-phase build system: initialization + incremental compilation.
"""

import subprocess
from pathlib import Path
from typing import List, Optional, Tuple

from ci.compiler.project_manager import ProjectManager
from ci.util.boards import Board  # type: ignore
from ci.util.locked_print import locked_print


class PioRunCompiler:
    """Efficient compiler using pio run with persistent projects."""

    def __init__(self, build_dir: Optional[str] = None) -> None:
        """Initialize the pio run compiler."""
        self.build_dir = build_dir
        self.project_manager = ProjectManager(build_dir or ".build")

    def compile_examples(
        self,
        board: Board,
        examples: List[Path],
        defines: Optional[List[str]] = None,
        verbose: bool = False,
        clean_first: bool = False,
        incremental: bool = True,
        keep: bool = False,
    ) -> Tuple[bool, str]:
        """Compile examples using efficient pio run system."""

        # Phase 1: Initialize persistent project if needed
        success, msg = self.project_manager.initialize_project(
            board, self.build_dir, defines, clean_first
        )
        if not success:
            return False, f"Project initialization failed: {msg}"

        # Phase 2: Incremental compilation for each example
        for example in examples:
            locked_print(
                f"\n*** Building {example.name} for {board.board_name} (pio run) ***"
            )

            success, msg = self._compile_single_example(
                board, example, incremental, verbose, keep
            )
            if not success:
                return False, f"Failed to compile {example.name}: {msg}"

        return True, "All examples compiled successfully"

    def _compile_single_example(
        self, board: Board, example: Path, incremental: bool, verbose: bool, keep: bool
    ) -> Tuple[bool, str]:
        """Compile a single example with optimizations."""

        project_dir = self.project_manager.get_project_directory(board, self.build_dir)

        # Prepare example source
        success, msg = self.project_manager.prepare_example_source(example, project_dir)
        if not success:
            return False, msg

        # Run incremental build
        return self._run_pio_build(project_dir, board, incremental, verbose, keep)

    def _run_pio_build(
        self,
        project_dir: Path,
        board: Board,
        incremental: bool,
        verbose: bool,
        keep: bool,
    ) -> Tuple[bool, str]:
        """Run pio build with incremental optimizations."""

        cmd = ["pio", "run", "--project-dir", str(project_dir)]

        # Add incremental optimizations and/or keep artifacts by disabling auto-clean
        if incremental or keep:
            cmd.append("--disable-auto-clean")

        if verbose:
            cmd.append("--verbose")

        # Execute build
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode == 0:
            locked_print(f"Build successful for {board.board_name}")
            return True, "Build successful"
        else:
            error_msg = f"Build failed for {board.board_name}: {result.stderr}"
            if verbose:
                locked_print(f"Build output: {result.stdout}")
                locked_print(f"Build error: {result.stderr}")
            return False, error_msg

    def initialize_projects_parallel(
        self,
        boards: List[Board],
        defines: Optional[List[str]] = None,
        clean_first: bool = False,
    ) -> Tuple[bool, str]:
        """Initialize multiple projects in parallel (if needed in future)."""
        # For now, initialize sequentially
        # Future enhancement: use ThreadPoolExecutor for parallel init

        for board in boards:
            success, msg = self.project_manager.initialize_project(
                board, self.build_dir, defines, clean_first
            )
            if not success:
                return False, f"Failed to initialize {board.board_name}: {msg}"

        return True, "All projects initialized successfully"

    def clean_project(self, board: Board) -> Tuple[bool, str]:
        """Clean a specific project."""
        return self.project_manager.clean_project(board, self.build_dir)

    def clean_all_projects(self) -> None:
        """Clean all project directories."""
        self.project_manager.clean_all_projects(self.build_dir)
