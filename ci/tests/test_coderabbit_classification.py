"""Classification tests for the CodeRabbit review addressor.

The `security-flag` bucket is the one classification with teeth: the
address-reviews skill refuses to auto-fix it, and `--check` counts it as
unresolved, which blocks merge. So it has to be wrong in neither direction.

Both directions have failed in practice:

- Substring matching fired "rce" inside "Sou(rce)". Every guideline-sourced
  CodeRabbit comment ends with "_Source: Coding guidelines_", so ordinary nits
  became permanently-unresolved security flags and merge blocked forever.
- The whole-word fix for that then stopped matching the plurals and inflections
  real findings use -- "leaks secrets", "hardcoded credentials" -- silently
  demoting genuine security comments to auto-fixable.
"""

import pytest

from ci.tools.coderabbit_addressor import Comment


def _classify(body: str) -> str:
    return Comment(
        id=1,
        author="coderabbitai",
        body=body,
        path="ci/example.py",
        line=1,
        in_reply_to=None,
    ).classification


# Must reach a human. Inflected forms are the point: these are how findings are
# actually phrased, not the dictionary-form keyword.
SECURITY_BODIES = [
    "This leaks secrets to the log",
    "Hardcoded credentials in the diff",
    "This breaks authentication",
    "Missing authorization check",
    "Possible RCE risk here",
    "Critical data loss on this path",
    "use-after-free here",
    "An auth failure is unhandled",
    "argv injection via unsanitized input",
    "Potential buffer overflow",
    "out-of-bounds read",
]

# Ordinary prose that merely contains a keyword *inside* another word, or uses
# "author" in its everyday sense.
NON_SECURITY_BODIES = [
    "See the Source file for details",
    "This enforces ordering",
    "Frees resources properly",
    "The author of this patch should note",
    "_Source: Coding guidelines_",
]


@pytest.mark.parametrize("body", SECURITY_BODIES)
def test_security_comments_are_flagged(body: str) -> None:
    assert _classify(body) == "security-flag", f"should be security-flag: {body!r}"


@pytest.mark.parametrize("body", NON_SECURITY_BODIES)
def test_ordinary_prose_is_not_flagged(body: str) -> None:
    assert _classify(body) != "security-flag", f"should not be security-flag: {body!r}"


def test_boilerplate_does_not_drive_classification() -> None:
    """CodeRabbit's footer is keyword-rich but describes nothing."""
    body = (
        "nit: rename this variable\n"
        "<!-- fingerprinting:phantom -->\n"
        "<details><summary>Prompt for AI Agents</summary>\n"
        "Check the source for critical security issues.\n"
        "</details>\n"
        "_Source: Coding guidelines_"
    )
    assert _classify(body) == "style"


def test_suggestion_block_classifies_as_valid_fix() -> None:
    assert _classify("Consider:\n```suggestion\nx = 1\n```") == "valid-fix"
