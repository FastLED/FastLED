from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""Validate @filter/@end-filter blocks in sketch files.

This script:
1. Checks all sketch files for valid @filter/@end-filter syntax
2. Warns about sketches with C++ platform guards but no @filter comments
3. Reports any inconsistencies between filters and guards
"""

import re
import sys
from pathlib import Path

from ci.compiler.sketch_filter import parse_filter_from_sketch


def find_all_sketches(examples_dir: Path) -> list[Path]:
    """Find all .ino sketch files in examples directory."""
    return sorted(examples_dir.rglob("*.ino"))


def has_platform_guards(content: str) -> tuple[bool, list[str]]:
    """Check if sketch has C++ platform guards.

    Returns:
        Tuple of (has_guards: bool, guard_types: List[str])
        guard_types examples: ['SKETCH_HAS_LOTS_OF_MEMORY', 'ESP32', 'esp32s3']
    """
    guard_patterns: list[str] = [
        r"#if\s+!?SKETCH_HAS_LOTS_OF_MEMORY",
        r"#ifdef?\s+(ESP32|ESP8266|__AVR__|TEENSY|RP2040|STM32)",
        r"#if\s+defined\((ESP32|ESP8266|__AVR__|TEENSY|RP2040|STM32)",
        r"#if\s+defined\(CONFIG_IDF_TARGET",
        r"#if\s+IS_ESP32",
        r"#ifndef\s+(ESP32|ESP8266)",
    ]

    matches: list[str] = []
    for pattern in guard_patterns:
        if re.search(pattern, content, re.MULTILINE | re.IGNORECASE):
            matches.append(pattern)

    return len(matches) > 0, matches


def validate_sketch(ino_path: Path, examples_dir: Path) -> tuple[bool, list[str]]:
    """Validate a single sketch file.

    Returns:
        Tuple of (valid: bool, warnings: List[str])
    """
    warnings: list[str] = []
    relative_path = ino_path.relative_to(examples_dir)

    try:
        content = ino_path.read_text(encoding="utf-8")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        return False, [f"{relative_path}: Failed to read file: {e}"]

    # Check for filter block
    filter_obj = parse_filter_from_sketch(ino_path)

    # Check for platform guards
    has_guards, guard_types = has_platform_guards(content)

    # Warnings if guards present but no filter
    if has_guards and filter_obj is None:
        warnings.append(
            f"{relative_path}: Has C++ platform guards but no @filter/@end-filter block. "
            "Consider adding a filter comment for better CI performance."
        )

    # Warnings if filter present but no guards
    if filter_obj is not None and not has_guards:
        warnings.append(
            f"{relative_path}: Has @filter/@end-filter but no matching C++ platform guards. "
            "Consider adding defensive C++ guards for safety."
        )

    # Try to parse filter and catch any syntax errors
    try:
        if filter_obj and not filter_obj.is_empty():
            # Basic validation: require and exclude should not overlap too much
            for req_key, req_vals in filter_obj.require.items():
                for exc_key, exc_vals in filter_obj.exclude.items():
                    if req_key == exc_key:
                        # Same key in both require and exclude might be suspicious
                        req_str = ",".join(req_vals)
                        exc_str = ",".join(exc_vals)
                        warnings.append(
                            f"{relative_path}: Filter has same key '{req_key}' in both require and exclude "
                            f"(require={req_str}, exclude={exc_str}). This might be unintended."
                        )
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        return False, [f"{relative_path}: Error validating filter: {e}"]

    return True, warnings


def main() -> int:
    """Run validation on all sketches."""
    # Find project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent
    examples_dir = project_root / "examples"

    if not examples_dir.exists():
        print(f"ERROR: Examples directory not found: {examples_dir}")
        return 1

    print(f"Validating sketches in: {examples_dir}")
    print()

    sketches: list[Path] = find_all_sketches(examples_dir)
    print(f"Found {len(sketches)} sketch files\n")

    errors: list[str] = []
    warnings: list[str] = []
    valid_count = 0

    for ino_path in sketches:
        valid, msgs = validate_sketch(ino_path, examples_dir)
        if not valid:
            errors.extend(msgs)
        else:
            warnings.extend(msgs)
            valid_count += 1

    # Print results
    if errors:
        print("ERRORS:")
        error: str
        for error in errors:
            print(f"  ✗ {error}")
        print()

    if warnings:
        print("WARNINGS:")
        warning: str
        for warning in warnings:
            print(f"  ⚠️  {warning}")
        print()

    # Summary
    print("=" * 70)
    print("Validation complete:")
    print(f"  Valid sketches: {valid_count}/{len(sketches)}")
    if warnings:
        print(f"  Warnings: {len(warnings)}")
    if errors:
        print(f"  Errors: {len(errors)}")
        return 1
    else:
        if warnings:
            print("  (Run with filter comments to resolve warnings)")
        else:
            print("  All sketches properly validated!")
        return 0


if __name__ == "__main__":
    sys.exit(main())
