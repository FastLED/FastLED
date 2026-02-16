#!/usr/bin/env python3
"""Smart selector for unit tests and examples with fuzzy matching.

This module provides intelligent selection of unit tests or examples based on
user input with fuzzy matching capabilities. It can:
- Match exact test/example names
- Perform fuzzy matching on partial names
- Disambiguate when multiple matches are found
- Auto-detect whether the user wants a unit test or example
"""

from dataclasses import dataclass
from pathlib import Path


@dataclass
class TestMatch:
    """Represents a matched test or example."""

    name: str  # Test/example name (e.g., "string", "Animartrix")
    path: str  # Relative path from project root
    type: str  # "unit_test" or "example"
    score: float  # Match score (0.0-1.0, higher is better)
    length: int = 0  # Length for tie-breaking (shorter is better)


def _normalize_for_matching(s: str) -> str:
    """Normalize a string for fuzzy matching by treating spaces, underscores, and hyphens as equivalent.

    Args:
        s: String to normalize

    Returns:
        Normalized string with spaces/underscores/hyphens removed
    """
    # Replace common separators with empty string for matching
    # This allows "string interner" to match "string_interner"
    return s.replace(" ", "").replace("_", "").replace("-", "")


def _edit_distance(s1: str, s2: str) -> int:
    """Calculate Levenshtein edit distance between two strings.

    Args:
        s1: First string
        s2: Second string

    Returns:
        Edit distance (number of edits needed to transform s1 into s2)
    """
    if len(s1) < len(s2):
        return _edit_distance(s2, s1)

    if len(s2) == 0:
        return len(s1)

    # Use two rows for space efficiency
    previous_row = list(range(len(s2) + 1))
    current_row = [0] * (len(s2) + 1)

    for i, c1 in enumerate(s1):
        current_row[0] = i + 1
        for j, c2 in enumerate(s2):
            # Cost of insertion, deletion, substitution
            insertions = previous_row[j + 1] + 1
            deletions = current_row[j] + 1
            substitutions = previous_row[j] + (0 if c1 == c2 else 1)
            current_row[j + 1] = min(insertions, deletions, substitutions)
        previous_row, current_row = current_row, previous_row

    return previous_row[-1]


def _fuzzy_score(query: str, target: str) -> tuple[float, int]:
    """Calculate fuzzy match score between query and target strings.

    Score is based on:
    - Exact match: 1.0
    - Case-insensitive exact match: 0.95
    - Normalized match (spaces/underscores/hyphens equivalent): 0.9
    - Substring match: 0.8-0.9 (prefer shorter targets and start matches)
    - Case-insensitive substring match: 0.7-0.85 (prefer shorter targets)
    - Edit distance match: 0.5-0.69 (based on similarity ratio)
    - Character sequence match: 0.0-0.49 (based on coverage)

    Args:
        query: Search query string
        target: Target string to match against

    Returns:
        Tuple of (match_score, target_length) where score is 0.0-1.0 and
        length is used for tie-breaking (shorter is better)
    """
    if not query:
        return (0.0, len(target))

    query_lower = query.lower()
    target_lower = target.lower()
    target_len = len(target)

    # Exact match
    if query == target:
        return (1.0, target_len)

    # Case-insensitive exact match
    if query_lower == target_lower:
        return (0.95, target_len)

    # Normalized match (spaces, underscores, hyphens treated as equivalent)
    query_normalized = _normalize_for_matching(query_lower)
    target_normalized = _normalize_for_matching(target_lower)
    if query_normalized == target_normalized:
        return (0.9, target_len)

    # Substring match (case-sensitive)
    if query in target:
        # Prefer matches at the start and shorter targets
        if target.startswith(query):
            return (0.9, target_len)
        return (0.8, target_len)

    # Substring match (case-insensitive)
    if query_lower in target_lower:
        # Prefer matches at the start and shorter targets
        if target_lower.startswith(query_lower):
            return (0.85, target_len)
        return (0.7, target_len)

    # Normalized substring match
    if query_normalized in target_normalized:
        # Multi-word queries (with spaces/separators) should get higher priority
        # since they're more specific (e.g., "string intern" is more specific than "string")
        has_separator = " " in query or "_" in query or "-" in query

        # Prefer matches at the start and shorter targets
        if target_normalized.startswith(query_normalized):
            # Boost score for multi-word queries with start match
            return (0.85 if has_separator else 0.75, target_len)
        # Boost score for multi-word queries
        return (0.75 if has_separator else 0.65, target_len)

    # Edit distance match (for close matches)
    edit_dist = _edit_distance(query_lower, target_lower)
    max_len = max(len(query_lower), len(target_lower))
    if max_len > 0:
        similarity = 1.0 - (edit_dist / max_len)
        # Only use edit distance for reasonably close matches (> 50% similar)
        if similarity > 0.5:
            # Scale to 0.5-0.69 range (below substring matches, above sequence matches)
            edit_score = 0.5 + (similarity - 0.5) * 0.38
            return (edit_score, target_len)

    # Character sequence match
    # All characters from query must appear in order in target
    query_pos = 0
    for char in target_lower:
        if query_pos < len(query_lower) and char == query_lower[query_pos]:
            query_pos += 1

    if query_pos == len(query_lower):
        # All characters matched in sequence
        coverage = query_pos / len(target_lower)
        return (0.49 * coverage, target_len)

    return (0.0, target_len)


def _score_match(query: str, match: TestMatch) -> tuple[float, int]:
    """Calculate match score for a TestMatch, considering both name and path.

    Path-based queries get exact match bonus if they match the path structure.

    Args:
        query: Search query string
        match: TestMatch to score

    Returns:
        Tuple of (match_score, length) where score is 0.0-1.0 and length is for tie-breaking
    """
    # Normalize path separators in query for cross-platform support
    normalized_query = query.replace("\\", "/")

    # Try matching against the path if query contains path separators
    if "/" in normalized_query or "\\" in query:
        # Path-based query - try exact path match first
        if match.path == normalized_query:
            return (1.0, len(match.name))  # Exact path match

        # Try matching path without extension
        path_no_ext = match.path.replace(".cpp", "").replace(".ino", "")
        if path_no_ext == normalized_query:
            return (1.0, len(match.name))  # Exact path match (without extension)

        # Try fuzzy match on path
        path_score, path_len = _fuzzy_score(normalized_query, match.path)
        if path_score > 0.0:
            return (path_score, path_len)

    # Also try treating spaces as path separators for queries like "fl async" -> "fl/async"
    # This allows natural language queries to match directory structures
    if " " in query and "/" not in query:
        space_to_slash = query.replace(" ", "/")

        # Try exact path match with space-to-slash conversion
        path_no_ext = match.path.replace(".cpp", "").replace(".ino", "")
        if space_to_slash in path_no_ext:
            # Substring match in path - check if it's a component match
            # e.g., "fl async" matches "tests/fl/async.cpp"
            parts = path_no_ext.split("/")
            query_parts = space_to_slash.split("/")

            # Check if query parts appear consecutively in path parts
            for i in range(len(parts) - len(query_parts) + 1):
                if parts[i : i + len(query_parts)] == query_parts:
                    # Exact component match - use name length for tie-breaking
                    return (0.95, len(match.name))

        # Try fuzzy match with space-to-slash conversion
        path_score, path_len = _fuzzy_score(space_to_slash, path_no_ext)
        if path_score > 0.0:
            # Boost score slightly since this is likely what the user meant
            return (min(1.0, path_score + 0.05), path_len)

    # Fall back to name-based matching
    return _fuzzy_score(query, match.name)


def _find_unit_tests(project_dir: Path) -> list[TestMatch]:
    """Find all unit test files in the tests directory.

    Args:
        project_dir: Project root directory

    Returns:
        List of TestMatch objects for unit tests
    """
    tests_dir = project_dir / "tests"
    if not tests_dir.exists():
        return []

    matches: list[TestMatch] = []

    # Find all .cpp files in tests directory
    for cpp_file in tests_dir.rglob("*.cpp"):
        # Get test name (filename without extension)
        test_name = cpp_file.stem

        # Get relative path from project root
        rel_path = cpp_file.relative_to(project_dir)

        matches.append(
            TestMatch(
                name=test_name,
                path=str(rel_path).replace("\\", "/"),
                type="unit_test",
                score=0.0,  # Will be calculated during matching
            )
        )

    return matches


def _find_examples(project_dir: Path) -> list[TestMatch]:
    """Find all examples in the examples directory.

    Args:
        project_dir: Project root directory

    Returns:
        List of TestMatch objects for examples
    """
    examples_dir = project_dir / "examples"
    if not examples_dir.exists():
        return []

    matches: list[TestMatch] = []

    # Find all .ino files in examples directory
    for ino_file in examples_dir.rglob("*.ino"):
        # Example name is the directory name
        example_name = ino_file.parent.name

        # Get relative path to example directory
        rel_path = ino_file.parent.relative_to(project_dir)

        matches.append(
            TestMatch(
                name=example_name,
                path=str(rel_path).replace("\\", "/"),
                type="example",
                score=0.0,  # Will be calculated during matching
            )
        )

    return matches


def smart_select(
    query: str, project_dir: Path | None = None, filter_type: str | None = None
) -> TestMatch | list[TestMatch]:
    """Smart selector for unit tests and examples with fuzzy matching.

    Returns either:
    - A single TestMatch if there's a clear best match
    - A list of TestMatch objects if disambiguation is needed

    Args:
        query: Search query (e.g., "Animartrix", "string", "async", "tests/fl/async")
        project_dir: Project root directory (defaults to current directory)
        filter_type: Optional type filter ("unit_test" or "example")

    Returns:
        Single TestMatch for auto-selection, or list of TestMatch for disambiguation
    """
    if project_dir is None:
        project_dir = Path.cwd()

    # Find all available tests and examples
    unit_tests = _find_unit_tests(project_dir)
    examples = _find_examples(project_dir)

    # Combine all matches (optionally filter by type)
    all_matches = unit_tests + examples
    if filter_type:
        all_matches = [m for m in all_matches if m.type == filter_type]

    # Calculate scores for all matches using both name and path
    scored_matches: list[TestMatch] = []
    for match in all_matches:
        score, length = _score_match(query, match)
        if score > 0.0:  # Only include matches with non-zero scores
            match.score = score
            match.length = length
            scored_matches.append(match)

    if not scored_matches:
        return []  # No matches found

    # Sort by score (descending), then by length (ascending) for tie-breaking
    # This ensures that when scores are equal, shorter names are preferred
    scored_matches.sort(key=lambda m: (-m.score, m.length))

    # Get top score
    top_score = scored_matches[0].score

    # Check if we have a clear winner
    # Exact match (score == 1.0) - auto-select unless there's another exact/near-exact match
    if top_score == 1.0:
        # Only disambiguate if there's another very high score (>= 0.98) of a different type
        # This handles true ambiguity like "async" matching both unit test and example "async"
        # But allows auto-selection when it's "async" (1.0) vs "Async" (0.95)
        top_type = scored_matches[0].type
        has_other_type_near_exact = False
        for match in scored_matches[1:]:
            if match.type != top_type and match.score >= 0.98:
                has_other_type_near_exact = True
                break

        if has_other_type_near_exact:
            # We have near-exact matches of different types - needs disambiguation
            pass  # Fall through to return multiple matches
        else:
            return scored_matches[0]  # Exact match wins

    # Case-insensitive exact match (score == 0.95) - check for conflicts
    if top_score == 0.95:
        # If we have multiple matches at 0.95, they're all case-insensitive exact matches
        # This means we have ambiguity (e.g., "async" matches both "Async" and "async")
        if len(scored_matches) > 1 and scored_matches[1].score == 0.95:
            # Multiple case-insensitive exact matches - needs disambiguation
            pass  # Fall through to return multiple matches
        else:
            # Single case-insensitive exact match
            return scored_matches[0]

    # For high scores (>= 0.7), check for significant margin over second place
    # Lower threshold for multi-word queries since they're more specific
    has_separator = " " in query or "_" in query or "-" in query
    min_score_threshold = 0.7 if has_separator else 0.8

    if top_score >= min_score_threshold:
        # Check for ties or close seconds
        if len(scored_matches) == 1:
            return scored_matches[0]  # Clear winner

        second_score = scored_matches[1].score

        # Handle exact ties - use length as tie-breaker (already sorted by length)
        if abs(top_score - second_score) < 0.01:  # Scores are essentially equal
            # If the top match is significantly shorter (>=30% shorter), auto-select it
            top_length = scored_matches[0].length
            second_length = scored_matches[1].length
            if top_length < second_length * 0.7:  # At least 30% shorter
                return scored_matches[0]  # Shortest match wins the tie
            # Otherwise, needs disambiguation
        else:
            # More aggressive margin for multi-word queries (0.1 vs 0.15)
            required_margin = 0.1 if has_separator else 0.15

            if top_score - second_score >= required_margin:
                return scored_matches[0]  # Clear winner with significant margin

    # Return top matches for disambiguation (up to 10 matches)
    return scored_matches[:10]


def format_match_for_display(match: TestMatch) -> str:
    """Format a TestMatch for display to the user.

    Args:
        match: TestMatch to format

    Returns:
        Formatted string for display
    """
    type_label = "example" if match.type == "example" else "unit test"
    return f"{match.name} ({type_label}) - {match.path}"


def get_best_match_or_prompt(
    query: str, project_dir: Path | None = None, filter_type: str | None = None
) -> TestMatch | None:
    """Get the best match for a query, or return None if disambiguation is needed.

    This is a convenience function that handles the logic of selecting the best
    match or determining when user input is needed.

    Args:
        query: Search query
        project_dir: Project root directory (defaults to current directory)
        filter_type: Optional type filter ("unit_test" or "example")

    Returns:
        TestMatch if auto-selected, None if disambiguation needed

    Side effects:
        Prints disambiguation options when multiple matches are found
    """
    result = smart_select(query, project_dir, filter_type)

    if isinstance(result, TestMatch):
        # Single match - auto-select
        return result

    if isinstance(result, list):
        if len(result) == 0:
            # No matches
            print(f"❌ No matches found for '{query}'")
            print()
            print("Try:")
            print("  - Check spelling")
            print("  - Use a longer query string")
            if filter_type == "unit_test":
                print("  - Use path-based query (e.g., 'tests/fl/async')")
                print("  - Run 'uv run test.py --list' to see all available tests")
                print("  - Run 'uv run test.py --list --cpp' to see only unit tests")
            elif filter_type == "example":
                print("  - Use path-based query (e.g., 'examples/Blink')")
                print("  - Run 'uv run test.py --list --examples' to see all examples")
            else:
                print(
                    "  - Use path-based query (e.g., 'tests/fl/async' or 'examples/Blink')"
                )
                print("  - Run 'uv run test.py --list' to see all available tests")
                print("  - Run 'uv run test.py --list --cpp' to see only unit tests")
                print("  - Run 'uv run test.py --list --examples' to see only examples")
            return None

        # Multiple matches - need disambiguation
        print(f"🔍 Multiple matches found for '{query}':")
        print()
        for i, match in enumerate(result, 1):
            print(
                f"  {i}. {format_match_for_display(match)} (score: {match.score:.2f})"
            )
        print()
        print("To disambiguate, use one of these methods:")
        print()
        # Show specific disambiguation examples based on the matches
        if result[0].type == "unit_test":
            # Show path-based query for the top match
            path_no_ext = result[0].path.replace(".cpp", "")
            print(f"  1. Use full path: uv run test.py {path_no_ext}")
            print(
                f"  2. Use relative path: uv run test.py {result[0].path.split('/', 1)[1] if '/' in result[0].path else result[0].name}"
            )
        else:  # example
            print(f"  1. Use full path: uv run test.py {result[0].path}")
            print(
                f"  2. Use example name with --examples: uv run test.py --examples {result[0].name}"
            )
        return None

    return None


def discover_all_tests(
    project_dir: Path | None = None,
) -> tuple[list[tuple[str, str]], list[tuple[str, str]]]:
    """Discover all available unit tests and examples.

    Args:
        project_dir: Project root directory (defaults to current directory)

    Returns:
        Tuple of (unit_tests, examples) where each is a list of (name, path) tuples
    """
    if project_dir is None:
        project_dir = Path.cwd()

    # Find all tests and examples
    unit_tests = _find_unit_tests(project_dir)
    examples = _find_examples(project_dir)

    # Convert to (name, path) tuples
    unit_test_tuples = [(m.name, m.path) for m in unit_tests]
    example_tuples = [(m.name, m.path) for m in examples]

    return (unit_test_tuples, example_tuples)
