#!/usr/bin/env python3
"""
FastLED All Source Build Script

This script implements the unified source build system for FastLED by:
1. Starting with ONE file for testing (allocator.cpp)
2. Copying original .cpp content to .cpp.hpp
3. Replacing .cpp with conditional wrapper code
4. Creating src/fastled_compile.cpp to include .cpp.hpp files
5. Adding FASTLED_ALL_SRC logic to compiler_control.h

CRITICAL: This script processes original .cpp content CORRECTLY:
- Step 1: Copy original .cpp -> .cpp.hpp (preserves source)
- Step 2: Replace .cpp with wrapper (conditional include)
"""

import os
import shutil
import glob
from pathlib import Path
from typing import List, Set


def find_cpp_files(base_dirs: List[str]) -> List[Path]:
    """Find all .cpp files in the specified directories and subdirectories."""
    cpp_files = []
    for base_dir in base_dirs:
        if not os.path.exists(base_dir):
            continue
        pattern = os.path.join(base_dir, "**", "*.cpp")
        cpp_files.extend(Path(f) for f in glob.glob(pattern, recursive=True))
    return sorted(cpp_files)


def get_relative_include_path(file_path: Path, from_src: bool = True) -> str:
    """Get the relative include path from src/ directory."""
    if from_src:
        # Remove 'src/' prefix if present
        parts = file_path.parts
        if parts[0] == 'src':
            return '/'.join(parts[1:])
        return str(file_path)
    return str(file_path)


def create_cpp_wrapper(original_cpp: Path, hpp_file: Path) -> str:
    """Create the wrapper .cpp file content with conditional includes."""
    relative_hpp_path = get_relative_include_path(hpp_file)
    
    content = f'''#include "fl/compiler_control.h"

#if !FASTLED_ALL_SRC
#include "{relative_hpp_path}"
#endif
'''
    return content


def update_compiler_control_h(compiler_control_path: Path) -> None:
    """Add FASTLED_ALL_SRC logic to compiler_control.h."""
    with open(compiler_control_path, 'r') as f:
        content = f.read()
    
    # Check if FASTLED_ALL_SRC is already defined
    if 'FASTLED_ALL_SRC' in content:
        print(f"FASTLED_ALL_SRC already defined in {compiler_control_path}")
        return
    
    # Add FASTLED_ALL_SRC logic at the end
    fastled_all_src_logic = '''

// All Source Build Control
// When FASTLED_ALL_SRC is enabled, all source is compiled into a single translation unit
// Debug/testing builds use individual compilation for better error isolation
#ifndef FASTLED_ALL_SRC
  #if defined(DEBUG) || defined(FASTLED_TESTING)
    #define FASTLED_ALL_SRC 0
  #elif !defined(RELEASE) || (RELEASE == 0)
    #define FASTLED_ALL_SRC 1
  #else
    #define FASTLED_ALL_SRC 0
  #endif
#endif
'''
    
    updated_content = content + fastled_all_src_logic
    
    with open(compiler_control_path, 'w') as f:
        f.write(updated_content)
    
    print(f"Updated {compiler_control_path} with FASTLED_ALL_SRC logic")


def create_fastled_compile_cpp(cpp_hpp_files: List[Path], output_path: Path) -> None:
    """Create src/fastled_compile.cpp with all .cpp.hpp includes."""
    includes = []
    for hpp_file in sorted(cpp_hpp_files):
        relative_path = get_relative_include_path(hpp_file)
        includes.append(f'#include "{relative_path}"')
    
    content = f'''// FastLED All Source Build File
// This file includes all .cpp.hpp files for unified compilation
// Generated automatically by scripts/all_source_build.py

#include "fl/compiler_control.h"

#if FASTLED_ALL_SRC

{chr(10).join(includes)}

#endif // FASTLED_ALL_SRC
'''
    
    with open(output_path, 'w') as f:
        f.write(content)
    
    print(f"Created {output_path} with {len(includes)} includes")


def process_single_cpp_file(cpp_file: Path, dry_run: bool = False) -> Path:
    """
    Process a single .cpp file, converting it to the new format.
    
    CRITICAL LOGIC:
    1. Copy original .cpp content to .cpp.hpp (PRESERVES SOURCE)
    2. Replace .cpp with wrapper code (CONDITIONAL INCLUDE)
    
    Returns the .cpp.hpp file path.
    """
    hpp_file = cpp_file.with_suffix('.cpp.hpp')
    
    print(f"Processing {cpp_file} -> {hpp_file}")
    
    if not dry_run:
        # STEP 1: Copy original .cpp content to .cpp.hpp (PRESERVE ORIGINAL SOURCE)
        print(f"  Step 1: Copying original content {cpp_file} -> {hpp_file}")
        shutil.copy2(cpp_file, hpp_file)
        
        # STEP 2: Replace .cpp with wrapper code (CONDITIONAL INCLUDE)
        print(f"  Step 2: Replacing {cpp_file} with wrapper code")
        wrapper_content = create_cpp_wrapper(cpp_file, hpp_file)
        with open(cpp_file, 'w') as f:
            f.write(wrapper_content)
    
    return hpp_file


def process_cpp_files(cpp_files: List[Path], dry_run: bool = False) -> List[Path]:
    """Process all .cpp files, converting them to the new format."""
    processed_hpp_files = []
    
    for cpp_file in cpp_files:
        hpp_file = process_single_cpp_file(cpp_file, dry_run)
        processed_hpp_files.append(hpp_file)
    
    return processed_hpp_files


def main():
    """Main function to run the all source build transformation."""
    import argparse
    
    parser = argparse.ArgumentParser(description='FastLED All Source Build Script')
    parser.add_argument('--dry-run', action='store_true', 
                       help='Show what would be done without making changes')
    parser.add_argument('--src-dir', default='src', 
                       help='Source directory (default: src)')
    parser.add_argument('--single-file', action='store_true',
                       help='Process only allocator.cpp for testing')
    
    args = parser.parse_args()
    
    # Define target directories
    target_dirs = [
        os.path.join(args.src_dir, 'fl'),
        os.path.join(args.src_dir, 'sensors'), 
        os.path.join(args.src_dir, 'fx')
    ]
    
    # Check if directories exist
    for dir_path in target_dirs:
        if not os.path.exists(dir_path):
            print(f"Warning: Directory {dir_path} does not exist")
    
    # Find all .cpp files
    all_cpp_files = find_cpp_files(target_dirs)
    
    # If single-file mode, only process allocator.cpp
    if args.single_file:
        allocator_files = [f for f in all_cpp_files if f.name == 'allocator.cpp']
        if not allocator_files:
            print("ERROR: allocator.cpp not found!")
            return
        cpp_files = allocator_files
        print(f"SINGLE FILE MODE: Processing only {cpp_files[0]}")
    else:
        cpp_files = all_cpp_files
        print(f"Found {len(cpp_files)} .cpp files:")
        for cpp_file in cpp_files:
            print(f"  {cpp_file}")
    
    if not cpp_files:
        print("No .cpp files found!")
        return
    
    if args.dry_run:
        print("\n--- DRY RUN MODE ---")
        print("The following actions would be performed:")
        
        for cpp_file in cpp_files:
            hpp_file = cpp_file.with_suffix('.cpp.hpp')
            print(f"  1. Copy {cpp_file} -> {hpp_file} (preserve original)")
            print(f"  2. Replace {cpp_file} with conditional wrapper")
        
        print(f"  3. Update {args.src_dir}/fl/compiler_control.h")
        print(f"  4. Create {args.src_dir}/fastled_compile.cpp")
        return
    
    print(f"\nStarting all source build transformation...")
    
    # Process .cpp files
    processed_hpp_files = process_cpp_files(cpp_files, dry_run=args.dry_run)
    
    # Update compiler_control.h
    compiler_control_path = Path(args.src_dir) / 'fl' / 'compiler_control.h'
    if compiler_control_path.exists():
        update_compiler_control_h(compiler_control_path)
    else:
        print(f"Warning: {compiler_control_path} not found")
    
    # Create fastled_compile.cpp
    fastled_compile_path = Path(args.src_dir) / 'fastled_compile.cpp'
    create_fastled_compile_cpp(processed_hpp_files, fastled_compile_path)
    
    print(f"\nAll source build transformation complete!")
    print(f"Processed {len(cpp_files)} .cpp files")
    print(f"Created {len(processed_hpp_files)} .cpp.hpp files")
    
    if args.single_file:
        print(f"\n*** SINGLE FILE MODE COMPLETE ***")
        print(f"Test with: bash test")
        print(f"If successful, run again without --single-file for full conversion")


if __name__ == '__main__':
    main() 
