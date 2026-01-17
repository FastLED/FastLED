from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


"""Sketch filtering system for selective compilation based on platform/memory constraints.

This module provides parsing and evaluation of @filter blocks in .ino files.

Supports multiple syntaxes:

YAML-style (multi-line):
    // @filter
    // require:
    //   - memory: high
    //   - platform: esp32
    // exclude:
    //   - target: esp32p4
    // @end-filter

One-liner (compact) - Natural language styles:
    // @filter (memory is high) and (platform is esp32s3)
    // @filter (mem is high) and (plat is esp32*)
    // @filter (memory: high) and (platform: esp32)
    // @filter (mem=high) and (plat=esp32)
    // @filter (mem:high) and (plat:esp32)
    // @filter (target is -D__AVR__) or (board is uno)

Optional colon after @filter - all styles work:
    // @filter: (mem is high)
    // @filter (mem is high)
    // @filter: (mem=high)
    // @filter (mem=high)

One-liner operators:
- is          : exact match (supports wildcards with *)
- is not      : negation
- =           : exact match (shorthand for 'is')
- :           : exact match (shorthand for 'is')
- matches     : regex/glob pattern match
- and         : logical AND
- or          : logical OR

Property name shortcuts:
- mem       : memory
- plat      : platform
- tgt       : target (for MCU target)
- brd       : board
"""

import fnmatch
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from ci.boards import Board


@dataclass
class SketchFilter:
    """Represents parsed @filter block from a sketch."""

    require: dict[str, list[str]] = field(default_factory=lambda: {})
    exclude: dict[str, list[str]] = field(default_factory=lambda: {})

    def is_empty(self) -> bool:
        """Check if filter has any constraints."""
        return not self.require and not self.exclude


def _normalize_property_name(key: str) -> str:
    """Normalize property name shortcuts to full names.

    Args:
        key: Property name (full or shorthand)

    Returns:
        Normalized property name
    """
    key_lower = key.lower().strip()
    shortcuts = {
        "mem": "memory",
        "plat": "platform",
        "tgt": "target",
        "brd": "board",
    }
    return shortcuts.get(key_lower, key_lower)


def parse_oneline_filter(filter_line: str) -> Optional[SketchFilter]:
    """Parse one-liner @filter syntax with multiple operator styles.

    Examples - all equivalent:
        // @filter (memory is high) and (platform is esp32s3)
        // @filter (mem is high) and (plat is esp32s3)
        // @filter (memory: high) and (platform: esp32s3)
        // @filter (mem=high) and (plat=esp32s3)
        // @filter (mem:high) and (plat:esp32s3)
        // @filter (target is -D__AVR__) or (memory is low)
        // @filter (platform is teensy) and not (board is teensylc)

    Supported operators:
    - is, is not  : exact match with wildcards
    - =           : shorthand for 'is'
    - :           : shorthand for 'is'
    - matches     : regex/glob match
    - not         : prefix for negation

    Args:
        filter_line: Content after '@filter' (without surrounding slashes/colon)

    Returns:
        SketchFilter object or None if parsing fails
    """
    filter_line = filter_line.strip()
    if not filter_line:
        return None

    require: dict[str, list[str]] = {}
    exclude: dict[str, list[str]] = {}

    # Pattern to match conditions in three formats:
    # 1. (key is value) or (key is not value) or (key matches value)
    # 2. (key = value) or (key=value)
    # 3. (key : value) or (key:value)
    # Pattern: (key OPERATOR value) where OPERATOR can be:
    #   - word operators (requires space): 'is not', 'is', 'matches'
    #   - symbol operators (optional space): '=' or ':'
    condition_pattern = r"\(\s*(\w+)(?:\s+(is\s+not|is|matches)|\s*([=:]))\s*([^)]+)\)"

    # Parse each condition
    found_any = False
    for match in re.finditer(condition_pattern, filter_line):
        key = match.group(1)
        word_operator = match.group(2)  # is, is not, matches (or None)
        match.group(3)  # =, : (or None)
        value = match.group(4)

        value = value.strip()

        # Determine operator type
        is_negated = False
        if word_operator:
            is_negated = word_operator == "is not"
        # symbol operators (= and :) are treated as 'is'

        # Check for 'not' prefix before this condition
        # Look backwards from the match start position to find 'not' keyword
        match_start = match.start()
        prefix_text = filter_line[:match_start].rstrip()
        if prefix_text.endswith("not"):
            is_negated = not is_negated  # Toggle negation if 'not' prefix found

        # Normalize key
        key = _normalize_property_name(key)
        if key not in ("platform", "target", "memory", "board"):
            continue

        # Parse value (may be -D__AVR__ or identifier or glob pattern)
        # Remove quotes if present
        if (value.startswith('"') and value.endswith('"')) or (
            value.startswith("'") and value.endswith("'")
        ):
            value = value[1:-1]

        target_dict = require if not is_negated else exclude
        target_dict.setdefault(key, []).append(value)
        found_any = True

    return SketchFilter(require=require, exclude=exclude) if found_any else None


def parse_filter_from_sketch(ino_path: Path) -> Optional[SketchFilter]:
    """Parse @filter block from .ino file.

    Supports both YAML-style (multi-line) and one-liner syntax.
    Colon after @filter is optional.
    Auto-detects format.

    Args:
        ino_path: Path to .ino file

    Returns:
        SketchFilter object if found, None otherwise

    Raises:
        ValueError: If filter syntax is invalid
    """
    try:
        content = ino_path.read_text(encoding="utf-8")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        return None

    # Try one-liner format first: // @filter: ... or // @filter ...
    # Colon is optional, so match both @filter: and @filter followed by (
    oneline_match = re.search(r"//\s*@filter\s*:?\s*\((.+?)$", content, re.MULTILINE)
    if oneline_match:
        # Re-match to capture everything including the first condition
        full_match = re.search(r"//\s*@filter\s*:?\s*(.+?)$", content, re.MULTILINE)
        if full_match:
            filter_line = full_match.group(1).strip()
            result = parse_oneline_filter(filter_line)
            if result:
                return result

    # Fall back to YAML-style multi-line format
    # Look for @filter ... @end-filter block (colon optional)
    match = re.search(
        r"//\s*@filter\s*:?\s*\n(.*?)//\s*@end-filter", content, re.DOTALL
    )
    if not match:
        return None

    filter_block = match.group(1)
    require: dict[str, list[str]] = {}
    exclude: dict[str, list[str]] = {}
    current_section: Optional[str] = None

    for line in filter_block.split("\n"):
        # Strip leading comment markers and whitespace
        line = line.lstrip("/").strip()

        if not line:
            continue

        # Detect section headers
        if line.startswith("require:"):
            current_section = "require"
            continue
        elif line.startswith("exclude:"):
            current_section = "exclude"
            continue

        # Parse list items under current section
        if line.startswith("- ") and current_section:
            try:
                # Format: "- key: value1|value2,value3"
                rest = line[2:].strip()
                if ":" not in rest:
                    raise ValueError(f"Invalid filter directive: {line}")

                key, values = rest.split(":", 1)
                key = _normalize_property_name(key.strip())
                values = values.strip()

                # Split by | or , for OR logic
                value_list = [v.strip() for v in re.split(r"[|,]", values) if v.strip()]

                if not value_list:
                    raise ValueError(f"No values provided for filter key: {key}")

                target_dict = require if current_section == "require" else exclude
                target_dict.setdefault(key, []).extend(value_list)
            except ValueError as e:
                raise ValueError(f"Error parsing filter line '{line}': {e}")

    return SketchFilter(require=require, exclude=exclude)


def should_skip_sketch(
    board: Board, sketch_filter: Optional[SketchFilter]
) -> tuple[bool, str]:
    """Determine if sketch should be skipped for this board based on filter.

    Args:
        board: Board configuration
        sketch_filter: Parsed filter from sketch, or None

    Returns:
        Tuple of (skip: bool, reason: str)
            skip=True means don't compile for this board
            reason explains why (or empty string if not skipped)
    """
    if sketch_filter is None or sketch_filter.is_empty():
        return False, ""

    # Check exclude conditions (skip if ANY match)
    for key, values in sketch_filter.exclude.items():
        board_value = _get_board_property(board, key)
        if board_value and _value_matches(board_value, values):
            return True, f"excluded by {key}={board_value}"

    # Check require conditions (skip if ALL don't match)
    for key, values in sketch_filter.require.items():
        board_value = _get_board_property(board, key)
        if not board_value or not _value_matches(board_value, values):
            return True, f"doesn't match required {key}={','.join(values)}"

    return False, ""


def _get_board_property(board: Board, key: str) -> Optional[str]:
    """Get board property for filter matching.

    Args:
        board: Board configuration
        key: Property name (memory, platform, target, board)

    Returns:
        Property value for comparison, or None if not applicable
    """
    if key == "memory":
        return board.memory_class
    elif key == "platform":
        return board.platform_family
    elif key == "target":
        return board.get_mcu_target()
    elif key == "board":
        return board.board_name
    else:
        return None


def _value_matches(board_value: str, filter_values: list[str]) -> bool:
    """Check if board value matches any of the filter values.

    Supports:
    - Exact matching: esp32 == esp32
    - Glob patterns: esp32* matches esp32, esp32s3, esp32c3
    - C++ defines: -D__AVR__ matches __AVR__

    Args:
        board_value: Board property value
        filter_values: List of acceptable values (OR logic)

    Returns:
        True if board_value matches any filter value (case-insensitive)
    """
    board_value_lower = board_value.lower()

    for filter_val in filter_values:
        filter_val_lower = filter_val.lower()

        # Handle C++ defines: -D__AVR__ should match __AVR__
        if filter_val_lower.startswith("-d"):
            filter_val_lower = filter_val_lower[2:]  # Remove -D prefix

        # Check for glob pattern (contains *)
        if "*" in filter_val_lower:
            if fnmatch.fnmatch(board_value_lower, filter_val_lower):
                return True
        # Exact match
        elif board_value_lower == filter_val_lower:
            return True

    return False


def format_filter_block(sketch_filter: SketchFilter) -> str:
    """Format filter as comment block for display/debugging.

    Args:
        sketch_filter: Filter to format

    Returns:
        Formatted string representation
    """
    lines = ["// @filter"]

    if sketch_filter.require:
        lines.append("// require:")
        for key, values in sorted(sketch_filter.require.items()):
            lines.append(f"//   - {key}: {','.join(values)}")

    if sketch_filter.exclude:
        lines.append("// exclude:")
        for key, values in sorted(sketch_filter.exclude.items()):
            lines.append(f"//   - {key}: {','.join(values)}")

    lines.append("// @end-filter")
    return "\n".join(lines)
