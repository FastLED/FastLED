#!/usr/bin/env python3
"""
Direct compiler for FastLED optimization.

This module provides direct compilation that bypasses PlatformIO entirely,
using captured metadata to compile sketches directly with the toolchain.
"""

import json
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional

from ci.util.boards import Board


@dataclass
class DirectCompilationResult:
    """Result of direct compilation."""
    success: bool
    output: str
    duration: float
    speedup: float


class DirectCompiler:
    """Direct compiler that bypasses PlatformIO using captured metadata."""
    
    def __init__(
        self,
        board: Board,
        metadata: dict,
        verbose: bool = False,
        additional_defines: Optional[List[str]] = None,
        additional_include_dirs: Optional[List[str]] = None
    ):
        self.board = board
        self.metadata = metadata
        self.verbose = verbose
        self.additional_defines = additional_defines or []
        self.additional_include_dirs = additional_include_dirs or []
        
    def compile(self, unity_cpp: Path, main_cpp: Path, example: str) -> DirectCompilationResult:
        """Compile the unity build directly using captured metadata."""
        start_time = time.time()
        
        try:
            # Find the unified archive
            unified_archive = unity_cpp.parent.parent / "libfastled_unified.a"
            if not unified_archive.exists():
                return DirectCompilationResult(
                    success=False,
                    output=f"Unified archive not found: {unified_archive}",
                    duration=0,
                    speedup=1
                )
            
            # Build compilation command using captured metadata
            compiler = self.metadata.get("compiler", "avr-g++")
            base_flags = self.metadata.get("base_flags", [])
            include_paths = self.metadata.get("include_paths", [])
            link_flags = self.metadata.get("link_flags", [])
            
            # Compile unity.cpp to object file
            unity_obj = unity_cpp.with_suffix(".o")
            compile_result = self._compile_source(
                compiler, unity_cpp, unity_obj, base_flags, include_paths
            )
            
            if not compile_result.success:
                return compile_result
            
            # Link against unified archive and framework libraries
            firmware_elf = unity_cpp.parent / "firmware.elf"
            link_result = self._link_executable(
                compiler, [unity_obj], firmware_elf, unified_archive, link_flags
            )
            
            if not link_result.success:
                return link_result
            
            # Generate hex file (if needed for Arduino)
            firmware_hex = firmware_elf.with_suffix(".hex")
            hex_result = self._generate_hex(firmware_elf, firmware_hex)
            
            duration = time.time() - start_time
            estimated_speedup = self.metadata.get("total_commands", 100) / 5  # Conservative estimate
            
            return DirectCompilationResult(
                success=True,
                output=f"Direct compilation successful: {firmware_hex}",
                duration=duration,
                speedup=estimated_speedup
            )
            
        except Exception as e:
            duration = time.time() - start_time
            return DirectCompilationResult(
                success=False,
                output=f"Direct compilation failed: {e}",
                duration=duration,
                speedup=1
            )
    
    def _compile_source(
        self, 
        compiler: str, 
        source_file: Path, 
        output_file: Path,
        base_flags: List[str],
        include_paths: List[str]
    ) -> DirectCompilationResult:
        """Compile a single source file to object file."""
        
        compile_cmd = [compiler]
        
        # Add base compilation flags
        compile_cmd.extend(base_flags)
        
        # Add include paths
        for include_path in include_paths:
            if include_path.startswith("-I"):
                compile_cmd.append(include_path)
            else:
                compile_cmd.append(f"-I{include_path}")
        
        # Add additional defines
        for define in self.additional_defines:
            if define.startswith("-D"):
                compile_cmd.append(define)
            else:
                compile_cmd.append(f"-D{define}")
        
        # Add additional include directories
        for include_dir in self.additional_include_dirs:
            if include_dir.startswith("-I"):
                compile_cmd.append(include_dir)
            else:
                compile_cmd.append(f"-I{include_dir}")
        
        # Compile to object file
        compile_cmd.extend([
            "-c",
            str(source_file),
            "-o",
            str(output_file)
        ])
        
        if self.verbose:
            print(f"Direct compile: {subprocess.list2cmdline(compile_cmd)}")
        
        try:
            result = subprocess.run(
                compile_cmd, 
                capture_output=True, 
                text=True,
                timeout=120  # 2 minute timeout
            )
            
            if result.returncode == 0:
                return DirectCompilationResult(True, "Compilation successful", 0, 1)
            else:
                return DirectCompilationResult(
                    False, 
                    f"Compilation failed: {result.stderr}", 
                    0, 
                    1
                )
                
        except subprocess.TimeoutExpired:
            return DirectCompilationResult(False, "Compilation timeout", 0, 1)
        except Exception as e:
            return DirectCompilationResult(False, f"Compilation error: {e}", 0, 1)
    
    def _link_executable(
        self,
        compiler: str,
        object_files: List[Path],
        output_file: Path,
        unified_archive: Path,
        link_flags: List[str]
    ) -> DirectCompilationResult:
        """Link object files with unified archive to create executable."""
        
        link_cmd = [compiler]
        
        # Add link flags
        link_cmd.extend(link_flags)
        
        # Add object files
        for obj_file in object_files:
            link_cmd.append(str(obj_file))
        
        # Add unified archive
        link_cmd.append(str(unified_archive))
        
        # Add standard Arduino libraries (math, etc.)
        link_cmd.extend(["-lm"])
        
        # Output file
        link_cmd.extend(["-o", str(output_file)])
        
        if self.verbose:
            print(f"Direct link: {subprocess.list2cmdline(link_cmd)}")
        
        try:
            result = subprocess.run(
                link_cmd,
                capture_output=True,
                text=True,
                timeout=60  # 1 minute timeout
            )
            
            if result.returncode == 0:
                return DirectCompilationResult(True, "Linking successful", 0, 1)
            else:
                return DirectCompilationResult(
                    False,
                    f"Linking failed: {result.stderr}",
                    0,
                    1
                )
                
        except subprocess.TimeoutExpired:
            return DirectCompilationResult(False, "Linking timeout", 0, 1)
        except Exception as e:
            return DirectCompilationResult(False, f"Linking error: {e}", 0, 1)
    
    def _generate_hex(self, elf_file: Path, hex_file: Path) -> DirectCompilationResult:
        """Generate Intel HEX file from ELF file."""
        
        # Find objcopy tool
        objcopy = "avr-objcopy"  # TODO: Should be derived from metadata
        
        hex_cmd = [
            objcopy,
            "-O", "ihex",
            "-R", ".eeprom",
            str(elf_file),
            str(hex_file)
        ]
        
        if self.verbose:
            print(f"Generate hex: {subprocess.list2cmdline(hex_cmd)}")
        
        try:
            result = subprocess.run(
                hex_cmd,
                capture_output=True,
                text=True,
                timeout=30
            )
            
            if result.returncode == 0:
                return DirectCompilationResult(True, "HEX generation successful", 0, 1)
            else:
                return DirectCompilationResult(
                    False,
                    f"HEX generation failed: {result.stderr}",
                    0,
                    1
                )
                
        except subprocess.TimeoutExpired:
            return DirectCompilationResult(False, "HEX generation timeout", 0, 1)
        except Exception as e:
            return DirectCompilationResult(False, f"HEX generation error: {e}", 0, 1)