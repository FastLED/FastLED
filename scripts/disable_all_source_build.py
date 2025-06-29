#!/usr/bin/env python3
"""
Disable All Source Build System

This script temporarily disables the all source build system by:
1. Removing wrapper logic from .cpp files
2. Restoring normal compilation behavior
3. Commenting out the FASTLED_ALL_SRC logic

This is a recovery tool to get the system back to a working state.
"""

import os
import glob
from pathlib import Path

def disable_wrapper_file(cpp_file: Path) -> bool:
    """Disable wrapper logic in a .cpp file by commenting out the conditional compilation."""
    try:
        with open(cpp_file, 'r') as f:
            content = f.read()
        
        # Check if it's a wrapper file (contains FASTLED_ALL_SRC logic)
        if 'FASTLED_ALL_SRC' in content and 'compiler_control.h' in content:
            print(f"Disabling wrapper: {cpp_file}")
            
            # Comment out the wrapper logic but keep the structure
            lines = content.split('\n')
            new_lines = []
            
            for line in lines:
                if any(keyword in line for keyword in ['#include "fl/compiler_control.h"', 
                                                       '#if !FASTLED_ALL_SRC', 
                                                       '#include', 
                                                       '#endif']):
                    new_lines.append(f"// DISABLED: {line}")
                else:
                    new_lines.append(line)
            
            # Add a simple placeholder implementation
            new_content = '\n'.join(new_lines)
            new_content += f'''

// TEMPORARY PLACEHOLDER - This file needs proper implementation
// All source build wrapper has been disabled to restore compilation
// TODO: Restore original {cpp_file.name} implementation

'''
            
            with open(cpp_file, 'w') as f:
                f.write(new_content)
            
            return True
        return False
        
    except Exception as e:
        print(f"Error processing {cpp_file}: {e}")
        return False

def disable_compiler_control():
    """Disable FASTLED_ALL_SRC logic in compiler_control.h."""
    control_file = Path("src/fl/compiler_control.h")
    if not control_file.exists():
        print(f"Warning: {control_file} not found")
        return
        
    with open(control_file, 'r') as f:
        content = f.read()
    
    if 'FASTLED_ALL_SRC' in content:
        print(f"Disabling FASTLED_ALL_SRC logic in {control_file}")
        
        # Comment out the entire FASTLED_ALL_SRC section
        lines = content.split('\n')
        new_lines = []
        in_all_src_section = False
        
        for line in lines:
            if 'All Source Build Control' in line:
                in_all_src_section = True
                new_lines.append(f"// DISABLED: {line}")
            elif in_all_src_section and line.strip() == '':
                in_all_src_section = False
                new_lines.append(line)
            elif in_all_src_section:
                new_lines.append(f"// DISABLED: {line}")
            else:
                new_lines.append(line)
        
        # Add a simple disable definition
        new_content = '\n'.join(new_lines)
        new_content += '''

// TEMPORARY DISABLE - All source build system disabled for recovery
#ifndef FASTLED_ALL_SRC
  #define FASTLED_ALL_SRC 0  // Always use individual compilation
#endif

'''
        
        with open(control_file, 'w') as f:
            f.write(new_content)

def main():
    """Main function to disable the all source build system."""
    print("🛑 Disabling All Source Build System for Recovery")
    
    # Find all .cpp files that might have wrapper code
    cpp_patterns = [
        "src/**/*.cpp",
    ]
    
    disabled_count = 0
    
    for pattern in cpp_patterns:
        for filepath in glob.glob(pattern, recursive=True):
            cpp_file = Path(filepath)
            if cpp_file.name != "fastled_compile.cpp":  # Don't modify the unified file
                if disable_wrapper_file(cpp_file):
                    disabled_count += 1
    
    # Disable compiler control logic
    disable_compiler_control()
    
    print(f"\n✅ All source build system disabled!")
    print(f"   - Disabled {disabled_count} wrapper files")
    print(f"   - Commented out FASTLED_ALL_SRC logic")
    print(f"\n⚠️  This is a TEMPORARY recovery state")
    print(f"   - System should now compile with individual .cpp files")
    print(f"   - You can now implement proper all source build system")

if __name__ == "__main__":
    main() 
