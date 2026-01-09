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
from typing import Optional


@dataclass
class TestMatch:
    """Represents a matched test or example."""

    name: str  # Test/example name (e.g., "string", "Animartrix")
    path: str  # Relative path from project root
    type: str  # "unit_test" or "example"
    score: float  # Match score (0.0-1.0, higher is better)


def _fuzzy_score(query: str, target: str) -> float:
    """Calculate fuzzy match score between query and target strings.

    Score is based on:
    - Exact match: 1.0
    - Case-insensitive exact match: 0.95
    - Substring match: 0.8
    - Case-insensitive substring match: 0.7
    - Character sequence match: 0.0-0.6 (based on coverage)

    Args:
        query: Search query string
        target: Target string to match against

    Returns:
        Match score from 0.0 (no match) to 1.0 (exact match)
    """
    if not query:
        return 0.0

    query_lower = query.lower()
    target_lower = target.lower()

    # Exact match
    if query == target:
        return 1.0

    # Case-insensitive exact match
    if query_lower == target_lower:
        return 0.95

    # Substring match (case-sensitive)
    if query in target:
        # Prefer matches at the start
        if target.startswith(query):
            return 0.9
        return 0.8

    # Substring match (case-insensitive)
    if query_lower in target_lower:
        # Prefer matches at the start
        if target_lower.startswith(query_lower):
            return 0.85
        return 0.7

    # Character sequence match
    # All characters from query must appear in order in target
    query_pos = 0
    for char in target_lower:
        if query_pos < len(query_lower) and char == query_lower[query_pos]:
            query_pos += 1

    if query_pos == len(query_lower):
        # All characters matched in sequence
        coverage = query_pos / len(target_lower)
        return 0.6 * coverage

    return 0.0


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
    query: str, project_dir: Path | None = None
) -> TestMatch | list[TestMatch]:
    """Smart selector for unit tests and examples with fuzzy matching.

    Returns either:
    - A single TestMatch if there's a clear best match
    - A list of TestMatch objects if disambiguation is needed

    Args:
        query: Search query (e.g., "Animartrix", "string", "async")
        project_dir: Project root directory (defaults to current directory)

    Returns:
        Single TestMatch for auto-selection, or list of TestMatch for disambiguation
    """
    if project_dir is None:
        project_dir = Path.cwd()

    # Find all available tests and examples
    unit_tests = _find_unit_tests(project_dir)
    examples = _find_examples(project_dir)

    # Combine all matches
    all_matches = unit_tests + examples

    # Calculate scores for all matches
    scored_matches: list[TestMatch] = []
    for match in all_matches:
        score = _fuzzy_score(query, match.name)
        if score > 0.0:  # Only include matches with non-zero scores
            match.score = score
            scored_matches.append(match)

    if not scored_matches:
        return []  # No matches found

    # Sort by score (descending)
    scored_matches.sort(key=lambda m: m.score, reverse=True)

    # Get top score
    top_score = scored_matches[0].score

    # Check if we have a clear winner
    # Exact match (score == 1.0) - but check for cross-type matches
    if top_score == 1.0:
        # Check if there are high-scoring matches of a different type
        # This handles cases like "async" (unit test) vs "Async" (example)
        top_type = scored_matches[0].type
        has_other_type_high_score = False
        for match in scored_matches[1:]:
            if match.type != top_type and match.score >= 0.9:
                has_other_type_high_score = True
                break

        if has_other_type_high_score:
            # We have high-scoring matches of different types - needs disambiguation
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

    # For high scores (>= 0.8), check for significant margin over second place
    if top_score >= 0.8:
        # Check for ties or close seconds
        if len(scored_matches) == 1:
            return scored_matches[0]  # Clear winner

        second_score = scored_matches[1].score
        if top_score - second_score >= 0.15:
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
    query: str, project_dir: Path | None = None
) -> TestMatch | None:
    """Get the best match for a query, or return None if disambiguation is needed.

    This is a convenience function that handles the logic of selecting the best
    match or determining when user input is needed.

    Args:
        query: Search query
        project_dir: Project root directory (defaults to current directory)

    Returns:
        TestMatch if auto-selected, None if disambiguation needed

    Side effects:
        Prints disambiguation options when multiple matches are found
    """
    result = smart_select(query, project_dir)

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
            print("  - Run 'bash test --examples' to see all examples")
            print("  - Run 'bash test --cpp' to see all unit tests")
            return None

        # Multiple matches - need disambiguation
        print(f"🔍 Multiple matches found for '{query}':")
        print()
        for i, match in enumerate(result, 1):
            print(
                f"  {i}. {format_match_for_display(match)} (score: {match.score:.2f})"
            )
        print()
        print("Please be more specific or use the full name/path.")
        return None

    return None
