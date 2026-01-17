#!/usr/bin/env python3
"""Replace #ifdef __cplusplus extern "C" patterns with FL_EXTERN_C macros."""

import re
from pathlib import Path

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


# Regex patterns for matching extern "C" blocks
PATTERN_OPEN = re.compile(
    r'#ifdef\s+__cplusplus\s*\n\s*extern\s+"C"\s*\{\s*\n\s*#endif', re.MULTILINE
)

PATTERN_CLOSE = re.compile(
    r"#ifdef\s+__cplusplus\s*\n\s*\}(?:\s*//[^\n]*)?\s*\n\s*#endif", re.MULTILINE
)

# Alternative patterns with different whitespace/comment variations
PATTERN_OPEN_ALT = re.compile(
    r'#ifdef\s+__cplusplus\s*\nextern\s+"C"\s*\{\s*\n#endif', re.MULTILINE
)

PATTERN_CLOSE_ALT = re.compile(
    r"#ifdef\s+__cplusplus\s*\n\}\s*(?://.*?)?\n#endif", re.MULTILINE
)

# Pattern for closing with comment variations
PATTERN_CLOSE_WITH_COMMENT = re.compile(
    r'#ifdef\s+__cplusplus\s*\n\s*\}\s*(?://\s*(?:extern\s+"C"|namespace|end))?\s*\n\s*#endif',
    re.MULTILINE,
)


def replace_extern_c_in_file(filepath: Path, dry_run: bool = False) -> tuple[bool, int]:
    """
    Replace extern "C" patterns in a file with FL_EXTERN_C macros.

    Args:
        filepath: Path to the file to process
        dry_run: If True, don't modify the file, just report what would change

    Returns:
        Tuple of (was_modified, replacement_count)
    """
    try:
        content = filepath.read_text(encoding="utf-8")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return False, 0

    original_content = content
    total_replacements = 0

    # Replace opening blocks
    content, count_open = PATTERN_OPEN.subn("FL_EXTERN_C_BEGIN", content)
    total_replacements += count_open

    if count_open == 0:
        # Try alternative pattern
        content, count_open = PATTERN_OPEN_ALT.subn("FL_EXTERN_C_BEGIN", content)
        total_replacements += count_open

    # Replace closing blocks - try most specific pattern first
    content, count_close = PATTERN_CLOSE_WITH_COMMENT.subn("FL_EXTERN_C_END", content)
    total_replacements += count_close

    if count_close == 0:
        content, count_close = PATTERN_CLOSE.subn("FL_EXTERN_C_END", content)
        total_replacements += count_close

    if count_close == 0:
        content, count_close = PATTERN_CLOSE_ALT.subn("FL_EXTERN_C_END", content)
        total_replacements += count_close

    # Warn if unbalanced
    if count_open != count_close:
        print(
            f"WARNING: Unbalanced replacements in {filepath}: {count_open} opens, {count_close} closes"
        )

    # Add include for compiler_control.h if we made replacements and it's not already included
    if total_replacements > 0:
        # Check if compiler_control.h is already included
        if "compiler_control.h" not in content:
            # Find a good place to add the include (after #pragma once or first include)
            pragma_match = re.search(r"#pragma once\s*\n", content)
            if pragma_match:
                insert_pos = pragma_match.end()
                content = (
                    content[:insert_pos]
                    + '#include "fl/compiler_control.h"\n'
                    + content[insert_pos:]
                )
            else:
                # Try to find first include
                first_include = re.search(r'#include\s+[<"]', content)
                if first_include:
                    insert_pos = first_include.start()
                    content = (
                        content[:insert_pos]
                        + '#include "fl/compiler_control.h"\n\n'
                        + content[insert_pos:]
                    )

    if content != original_content:
        if not dry_run:
            try:
                filepath.write_text(content, encoding="utf-8")
                print(f"✓ Modified {filepath} ({total_replacements} replacements)")
                handle_keyboard_interrupt_properly()
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                print(f"Error writing {filepath}: {e}")
                return False, 0
        else:
            print(f"Would modify {filepath} ({total_replacements} replacements)")
        return True, total_replacements

    return False, 0


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description='Replace extern "C" patterns with FL_EXTERN_C macros'
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be changed without modifying files",
    )
    parser.add_argument(
        "--exclude-third-party",
        action="store_true",
        default=True,
        help="Exclude third_party directories (default: True)",
    )
    parser.add_argument(
        "--include-third-party",
        action="store_false",
        dest="exclude_third_party",
        help="Include third_party directories",
    )
    parser.add_argument(
        "files",
        nargs="*",
        help="Specific files to process (if empty, processes all src/ files)",
    )

    args = parser.parse_args()

    root = Path(__file__).parent.parent

    if args.files:
        files_to_process = [Path(f) for f in args.files]
    else:
        # Find all .h, .hpp, .cpp files in src/
        patterns = ["**/*.h", "**/*.hpp", "**/*.cpp"]
        files_to_process: list[Path] = []
        for pattern in patterns:
            files_to_process.extend((root / "src").rglob(pattern))

    # Filter out third_party if requested
    if args.exclude_third_party:
        files_to_process = [f for f in files_to_process if "third_party" not in f.parts]

    total_files = len(files_to_process)
    modified_count = 0
    total_replacements = 0

    print(f"Processing {total_files} files...")
    if args.dry_run:
        print("DRY RUN - No files will be modified\n")

    for filepath in sorted(files_to_process):
        was_modified, count = replace_extern_c_in_file(filepath, dry_run=args.dry_run)
        if was_modified:
            modified_count += 1
            total_replacements += count

    print(
        f"\n{'Would modify' if args.dry_run else 'Modified'} {modified_count} of {total_files} files"
    )
    print(f"Total replacements: {total_replacements}")


if __name__ == "__main__":
    main()
