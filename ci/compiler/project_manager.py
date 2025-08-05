#!/usr/bin/env python3
"""
Project manager for persistent PlatformIO projects using pio run.
Handles project initialization, configuration, and lifecycle management.
"""

import os
import shutil
import subprocess
from pathlib import Path
from typing import Optional, Tuple

from ci.util.boards import Board  # type: ignore
from ci.util.locked_print import locked_print


class ProjectManager:
    """Manages persistent PlatformIO projects for efficient builds."""
    
    def __init__(self, base_dir: str = ".build") -> None:
        """Initialize project manager with base directory."""
        self.base_dir = Path(base_dir)
        self.projects_dir = self.base_dir / "projects"
        self.cache_dir = self.base_dir / "cache"
        
        # Ensure directories exist
        self.projects_dir.mkdir(parents=True, exist_ok=True)
        self.cache_dir.mkdir(parents=True, exist_ok=True)
    
    def get_project_directory(self, board: Board, build_dir: Optional[str] = None) -> Path:
        """Get the project directory for a board."""
        if build_dir:
            # Use pio-run subdirectory to avoid conflicts with existing build system
            return Path(build_dir) / "pio-run" / "projects" / board.board_name
        return self.projects_dir / board.board_name
    
    def is_project_initialized(self, project_dir: Path, board: Board) -> bool:
        """Check if a project is properly initialized."""
        platformio_ini = project_dir / "platformio.ini"
        src_dir = project_dir / "src"
        pio_dir = project_dir / ".pio"
        
        return (
            project_dir.exists() and
            platformio_ini.exists() and
            src_dir.exists() and
            pio_dir.exists()
        )
    
    def initialize_project(
        self, 
        board: Board, 
        build_dir: Optional[str] = None,
        defines: Optional[list[str]] = None,
        clean_first: bool = False
    ) -> Tuple[bool, str]:
        """Initialize a persistent project directory for a board."""
        project_dir = self.get_project_directory(board, build_dir)
        
        # Clean if requested
        if clean_first and project_dir.exists():
            locked_print(f"Cleaning project directory for {board.board_name}")
            shutil.rmtree(project_dir)
        
        # Skip if already initialized and valid
        if self.is_project_initialized(project_dir, board):
            return True, f"Project already initialized for {board.board_name}"
        
        locked_print(f"Initializing project for {board.board_name}")
        
        # Ensure project directory exists
        project_dir.mkdir(parents=True, exist_ok=True)
        
        # Create project structure
        cmd = [
            "pio", "project", "init",
            "--board", board.get_real_board_name(),
            "--project-dir", str(project_dir),
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            return False, f"Failed to init project for {board.board_name}: {result.stderr}"
        
        # Configure platformio.ini
        success, msg = self._configure_platformio_ini(project_dir, board, defines)
        if not success:
            return False, msg
        
        # Copy FastLED library to project lib directory
        success, msg = self._setup_fastled_library(project_dir)
        if not success:
            return False, msg
        
        # Initial build to establish dependencies
        success, msg = self._run_initial_build(project_dir, board)
        if not success:
            return False, msg
        
        return True, f"Successfully initialized project for {board.board_name}"
    
    def _configure_platformio_ini(
        self, 
        project_dir: Path, 
        board: Board, 
        defines: Optional[list[str]] = None
    ) -> Tuple[bool, str]:
        """Configure platformio.ini for the project."""
        platformio_ini = project_dir / "platformio.ini"
        
        try:
            # Get the FastLED source path as absolute path for symlink
            src_path = Path(__file__).parent.parent.parent / "src"
            fastled_src_path = src_path.resolve()
            
            # Read existing platformio.ini
            content = platformio_ini.read_text()
            
            # Add FastLED configuration
            fastled_config = f"""
# FastLED library dependency (symlinked for efficiency)
lib_deps = symlink://{fastled_src_path}

# LDF Configuration (will be dynamically changed)
lib_ldf_mode = deep+
lib_compat_mode = no

# Build optimization
"""
            
            # Add defines if provided
            if defines:
                for define in defines:
                    fastled_config += f"build_flags = -D{define}\n"
            
            # Append FastLED config to existing content
            content += fastled_config
            
            # Write back
            platformio_ini.write_text(content)
            
            return True, "platformio.ini configured successfully"
            
        except Exception as e:
            return False, f"Failed to configure platformio.ini: {e}"
    
    def _setup_fastled_library(self, project_dir: Path) -> Tuple[bool, str]:
        """Setup FastLED library in the project."""
        try:
            # Get the FastLED source path
            src_path = Path(__file__).parent.parent.parent / "src"
            
            # Create lib directory in project
            lib_dir = project_dir / "lib"
            lib_dir.mkdir(exist_ok=True)
            
            # Create FastLED directory in lib
            fastled_lib_dir = lib_dir / "FastLED"
            if fastled_lib_dir.exists():
                shutil.rmtree(fastled_lib_dir)
            
            # Copy FastLED source to lib directory
            shutil.copytree(src_path, fastled_lib_dir)
            
            # Create a library.properties file for Arduino compatibility
            library_properties = fastled_lib_dir / "library.properties"
            library_properties.write_text("""name=FastLED
version=3.7.1
author=Daniel Garcia
maintainer=Daniel Garcia
sentence=FastLED library for controlling addressable LED strips
paragraph=FastLED is a fast, efficient, easy-to-use Arduino library for controlling many different types of addressable LED stripsets
category=Display
url=https://github.com/FastLED/FastLED
architectures=*
""")
            
            # Create library.json for PlatformIO compatibility
            library_json = fastled_lib_dir / "library.json"
            library_json.write_text("""{
  "name": "FastLED",
  "version": "3.7.1",
  "description": "FastLED library for controlling addressable LED strips",
  "homepage": "https://github.com/FastLED/FastLED",
  "frameworks": ["arduino"],
  "platforms": "*"
}""")
            
            return True, "FastLED library setup successful"
            
        except Exception as e:
            return False, f"Failed to setup FastLED library: {e}"
    
    def _run_initial_build(self, project_dir: Path, board: Board) -> Tuple[bool, str]:
        """Run initial build to establish dependencies."""
        locked_print(f"Running initial build for {board.board_name} to establish dependencies")
        
        # Create a dummy main.cpp for initial build
        src_dir = project_dir / "src"
        src_dir.mkdir(exist_ok=True)
        
        dummy_main = src_dir / "main.cpp"
        dummy_main.write_text("""
#include <Arduino.h>
void setup() {}
void loop() {}
""")
        
        cmd = [
            "pio", "run", 
            "--project-dir", str(project_dir),
            "--environment", board.get_real_board_name()  # Specify environment
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode == 0:
            locked_print(f"Initial build successful for {board.board_name}")
            return True, "Initial build successful"
        else:
            return False, f"Initial build failed for {board.board_name}: {result.stderr}"
    
    def prepare_example_source(self, example_path: Path, project_dir: Path) -> Tuple[bool, str]:
        """Copy example source to project src directory."""
        src_dir = project_dir / "src"
        
        try:
            # Clear existing source files
            if src_dir.exists():
                shutil.rmtree(src_dir)
            src_dir.mkdir(parents=True, exist_ok=True)
            
            if example_path.is_file():
                # Single .ino file
                if example_path.suffix == ".ino":
                    # Copy as main.cpp for PlatformIO
                    shutil.copy2(example_path, src_dir / "main.cpp")
                else:
                    shutil.copy2(example_path, src_dir / example_path.name)
            else:
                # Directory with multiple files
                for file_path in example_path.rglob("*"):
                    if file_path.is_file():
                        rel_path = file_path.relative_to(example_path)
                        dest_path = src_dir / rel_path
                        dest_path.parent.mkdir(parents=True, exist_ok=True)
                        
                        if file_path.suffix == ".ino":
                            # Rename .ino to .cpp for PlatformIO
                            dest_path = dest_path.with_suffix(".cpp")
                            if file_path.name == example_path.name + ".ino":
                                # Main sketch file becomes main.cpp
                                dest_path = src_dir / "main.cpp"
                        
                        shutil.copy2(file_path, dest_path)
            
            return True, "Example source prepared successfully"
            
        except Exception as e:
            return False, f"Failed to prepare example source: {e}"
    
    def clean_project(self, board: Board, build_dir: Optional[str] = None) -> Tuple[bool, str]:
        """Clean project build artifacts."""
        project_dir = self.get_project_directory(board, build_dir)
        
        try:
            cmd = ["pio", "run", "--target", "clean", "--project-dir", str(project_dir)]
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode == 0:
                return True, f"Cleaned project for {board.board_name}"
            else:
                return False, f"Failed to clean project for {board.board_name}: {result.stderr}"
                
        except Exception as e:
            return False, f"Exception cleaning project for {board.board_name}: {e}"
    
    def clean_all_projects(self, build_dir: Optional[str] = None) -> None:
        """Clean all project directories."""
        projects_dir = Path(build_dir) / "projects" if build_dir else self.projects_dir
        
        if projects_dir.exists():
            locked_print("Cleaning all project directories")
            shutil.rmtree(projects_dir)
            projects_dir.mkdir(parents=True, exist_ok=True)
