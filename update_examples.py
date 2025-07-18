#!/usr/bin/env python3
"""
Script to update examples directory with modern FastLED patterns

This script applies two transformations to all example files:
1. Header replacement: #include "fl/ptr.h" -> #include "fl/memory.h"
2. Usage pattern replacement: 
   - fl::NewPtr<T>(...) -> fl::make_shared<T>(...)
   - NewPtr<T>(...) -> fl::make_shared<T>(...)
   - TypePtr::New(...) -> fl::make_shared<Type>(...)
   - fl::Ptr<T> -> fl::shared_ptr<T>
"""

import re
import os
import argparse

def update_file_content(content, filepath):
    """Apply all transformations to file content"""
    original_content = content
    
    # 1. Header replacement: fl/ptr.h -> fl/memory.h
    content = re.sub(r'#include\s+"fl/ptr\.h"', '#include "fl/memory.h"', content)
    
    # 2. Replace fl::NewPtr<Type>(...) with fl::make_shared<Type>(...)
    content = re.sub(r'\bfl::NewPtr<([^>]+)>\s*\(', r'fl::make_shared<\1>(', content)
    
    # 3. Replace NewPtr<Type>(...) with fl::make_shared<Type>(...) (for cases without fl:: prefix)
    content = re.sub(r'\bNewPtr<([^>]+)>\s*\(', r'fl::make_shared<\1>(', content)
    
    # 4. Replace TypePtr::New(...) with fl::make_shared<Type>(...)
    def replace_type_ptr_new(match):
        type_ptr = match.group(1)
        args = match.group(2)
        if type_ptr.endswith('Ptr'):
            base_type = type_ptr[:-3]  # Remove 'Ptr' suffix
            return f'fl::make_shared<{base_type}>({args})'
        return match.group(0)  # No change if doesn't end with Ptr
    
    content = re.sub(r'\b(\w+Ptr)::New\s*\(([^)]*)\)', replace_type_ptr_new, content)
    
    # 5. Replace fl::Ptr<Type> with fl::shared_ptr<Type>
    def replace_ptr(match):
        return match.group(0).replace('fl::Ptr<', 'fl::shared_ptr<')
    
    # Pattern to match fl::Ptr<...> with proper template nesting
    content = re.sub(r'\bfl::Ptr<(?:[^<>]|<(?:[^<>]|<[^<>]*>)*>)*>', replace_ptr, content)
    
    # 6. Also handle typedef/using declarations that might reference Ptr
    content = re.sub(r'\bPtr<([^>]+)>', r'shared_ptr<\1>', content)
    
    return content, content != original_content

def update_file(filepath, dry_run=False):
    """Update a single file"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        updated_content, changed = update_file_content(content, filepath)
        
        if changed:
            if dry_run:
                print(f"Would update: {filepath}")
                return True
            else:
                with open(filepath, 'w', encoding='utf-8') as f:
                    f.write(updated_content)
                print(f"Updated: {filepath}")
                return True
        else:
            print(f"No changes: {filepath}")
            return False
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        return False

def find_files(directory, extensions):
    """Find all files with given extensions in directory"""
    files = []
    for root, _, filenames in os.walk(directory):
        for filename in filenames:
            if any(filename.endswith(ext) for ext in extensions):
                files.append(os.path.join(root, filename))
    return files

def main():
    parser = argparse.ArgumentParser(description='Update examples with modern FastLED patterns')
    parser.add_argument('--directory', default='examples', help='Directory to process')
    parser.add_argument('--files', nargs='+', help='Specific files to process')
    parser.add_argument('--dry-run', action='store_true', help='Show what would be changed without making changes')
    parser.add_argument('--extensions', nargs='+', default=['.ino', '.h', '.cpp', '.hpp'], help='File extensions to process')
    
    args = parser.parse_args()
    
    if args.files:
        files = args.files
    elif args.directory:
        files = find_files(args.directory, args.extensions)
    else:
        print("Error: Must specify either --directory or --files")
        return 1
    
    total_files = len(files)
    updated_count = 0
    no_change_count = 0
    
    for filepath in files:
        if update_file(filepath, args.dry_run):
            updated_count += 1
        else:
            no_change_count += 1
    
    print(f"\nSummary:")
    print(f"  Total files: {total_files}")
    print(f"  {'Would update' if args.dry_run else 'Updated'}: {updated_count}")
    print(f"  No changes: {no_change_count}")
    
    return 0

if __name__ == '__main__':
    exit(main()) 
