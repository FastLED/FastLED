"""Real-world test examples for the enhanced @filter syntax.

This script validates all the new @filter syntax styles with actual sketch files.
"""

import tempfile
from pathlib import Path
from typing import Any

import pytest

from ci.compiler.sketch_filter import parse_filter_from_sketch


def test_example_sketches() -> None:
    """Test various @filter syntax styles with example sketches."""

    examples: list[dict[str, Any]] = [
        # Style 1: Original - with colon and 'is' operator
        {
            "name": "Original with colon (backward compat)",
            "content": """// @filter: (memory is high)
void setup() {}
void loop() {}
""",
            "expected": {"require": {"memory": ["high"]}, "exclude": {}},
        },
        # Style 2: Natural language - without colon, 'is' operator
        {
            "name": "Natural language without colon",
            "content": """// @filter (memory is high)
void setup() {}
void loop() {}
""",
            "expected": {"require": {"memory": ["high"]}, "exclude": {}},
        },
        # Style 3: Using = operator with shorthand
        {
            "name": "Shorthand with = operator",
            "content": """// @filter (mem=high)
void setup() {}
void loop() {}
""",
            "expected": {"require": {"memory": ["high"]}, "exclude": {}},
        },
        # Style 4: Using : operator with shorthand
        {
            "name": "Shorthand with : operator",
            "content": """// @filter (mem:high)
void setup() {}
void loop() {}
""",
            "expected": {"require": {"memory": ["high"]}, "exclude": {}},
        },
        # Style 5: Complex - multiple conditions with shortcuts
        {
            "name": "Complex with shortcuts and operators",
            "content": """// @filter (mem is high) and (plat=esp32*) and (tgt is not esp32p4)
void setup() {}
void loop() {}
""",
            "expected": {
                "require": {"memory": ["high"], "platform": ["esp32*"]},
                "exclude": {"target": ["esp32p4"]},
            },
        },
        # Style 6: Mixed operators
        {
            "name": "Mixed operators",
            "content": """// @filter (mem=high) and (plat:esp32s3)
void setup() {}
void loop() {}
""",
            "expected": {
                "require": {"memory": ["high"], "platform": ["esp32s3"]},
                "exclude": {},
            },
        },
        # Style 7: With spaces around operators
        {
            "name": "Spaces around operators",
            "content": """// @filter (mem = high) and (plat : esp32)
void setup() {}
void loop() {}
""",
            "expected": {
                "require": {"memory": ["high"], "platform": ["esp32"]},
                "exclude": {},
            },
        },
        # Style 8: YAML style with shortcuts
        {
            "name": "YAML style with shortcuts",
            "content": """// @filter
// require:
//   - mem: high
//   - plat: esp32
// exclude:
//   - tgt: esp32p4
// @end-filter
void setup() {}
void loop() {}
""",
            "expected": {
                "require": {"memory": ["high"], "platform": ["esp32"]},
                "exclude": {"target": ["esp32p4"]},
            },
        },
        # Style 9: YAML without colon after @filter
        {
            "name": "YAML without colon",
            "content": """// @filter
// require:
//   - memory: high
// @end-filter
void setup() {}
void loop() {}
""",
            "expected": {"require": {"memory": ["high"]}, "exclude": {}},
        },
    ]

    failed_examples: list[str] = []

    for example in examples:
        with tempfile.NamedTemporaryFile(mode="w", suffix=".ino", delete=False) as f:
            content: str = example["content"]
            f.write(content)
            f.flush()
            temp_path = f.name

        try:
            result = parse_filter_from_sketch(Path(temp_path))

            if result is None:
                require: dict[str, list[str]] = {}
                exclude: dict[str, list[str]] = {}
            else:
                require = result.require
                exclude = result.exclude

            expected: dict[str, Any] = example["expected"]
            expected_require: dict[str, list[str]] = expected["require"]
            expected_exclude: dict[str, list[str]] = expected["exclude"]

            if require != expected_require or exclude != expected_exclude:
                failed_examples.append(
                    f"{example['name']}: "
                    f"expected require={expected_require}, exclude={expected_exclude}, "
                    f"got require={require}, exclude={exclude}"
                )

        finally:
            Path(temp_path).unlink(missing_ok=True)

    assert not failed_examples, "Failed examples: " + "; ".join(failed_examples)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
