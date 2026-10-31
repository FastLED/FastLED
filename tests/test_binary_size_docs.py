"""Entry page for bloat work must stay cheap to read.

Agents investigating flash/RAM read `agents/docs/binary-size-analysis.md` once
per session (21 reads across 20 sessions in one 24h sample, 18 KB each). The
page is split so the entry page answers the usual question and the long-form
material is opt-in. These tests are the ratchet: an entry page that grows back
into a reference re-introduces the cost on every future session, and a
scorecard that is not reachable from the entry page is invisible.
"""

import re
from pathlib import Path


PROJECT_ROOT = Path(__file__).parent.parent
DOCS = PROJECT_ROOT / "agents/docs"
ENTRY = DOCS / "binary-size-analysis.md"
SCORECARDS = DOCS / "binary-size-scorecards.md"
REFERENCE = DOCS / "binary-size-reference.md"

ENTRY_CEILING_BYTES = 6000


def _entry() -> str:
    return ENTRY.read_text(encoding="utf-8")


def test_entry_page_stays_within_its_byte_ceiling() -> None:
    size = ENTRY.stat().st_size
    assert size <= ENTRY_CEILING_BYTES, (
        f"{ENTRY.name} is {size} B, over the {ENTRY_CEILING_BYTES} B ceiling. "
        "Move long-form material to binary-size-reference.md or "
        "binary-size-scorecards.md instead of growing the entry page."
    )


def test_entry_page_links_both_companion_pages() -> None:
    entry = _entry()
    assert "(binary-size-scorecards.md)" in entry
    assert "(binary-size-reference.md)" in entry


def test_entry_page_carries_no_per_issue_scorecard_prose() -> None:
    entry = _entry()
    assert "scorecard:" not in entry
    assert not re.search(r"(?m)^### #\d{4}", entry)


def test_every_scorecard_is_reachable_from_the_entry_table() -> None:
    """A scorecard with no row in the entry table is work an agent cannot find."""
    entry = _entry()
    issues = re.findall(r"(?m)^### #(\d+) ", SCORECARDS.read_text(encoding="utf-8"))
    assert issues, "no scorecards parsed - did the heading format change?"
    unreachable = [issue for issue in issues if f"#{issue}" not in entry]
    assert not unreachable, (
        "scorecards exist for issues with no row in the entry page's "
        f"'Current per-board readings' table: {unreachable}"
    )


def test_entry_table_budget_files_exist() -> None:
    entry = _entry()
    budgets = set(re.findall(r"`(tests/data/[a-z0-9_]+\.json)`", entry))
    assert budgets, "entry table lists no budget files"
    missing = sorted(b for b in budgets if not (PROJECT_ROOT / b).is_file())
    assert not missing, f"entry table cites budget files that do not exist: {missing}"


def _headings(path: Path) -> set[str]:
    """GitHub heading anchors, near enough for the headings these pages use."""
    found = set()
    for heading in re.findall(
        r"(?m)^\s{0,3}#{1,6} .*$", path.read_text(encoding="utf-8")
    ):
        text = heading.strip().lstrip("#").strip().lower()
        found.add(re.sub(r"[^a-z0-9 \-_]", "", text).replace(" ", "-"))
    return found


def test_relative_links_in_the_doc_split_resolve() -> None:
    broken = []
    for doc in (ENTRY, SCORECARDS, REFERENCE):
        for target in re.findall(r"\]\(([^)]+)\)", doc.read_text(encoding="utf-8")):
            if target.startswith(("http://", "https://", "mailto:")):
                continue
            path, _, fragment = target.partition("#")
            target_doc = doc if not path else doc.parent / path
            if not target_doc.exists():
                broken.append(f"{doc.name} -> {target} (missing file)")
            elif fragment and fragment not in _headings(target_doc):
                broken.append(f"{doc.name} -> {target} (missing anchor)")
    assert not broken, f"broken relative links: {broken}"


def test_reference_page_keeps_the_flash_lever_table() -> None:
    reference = REFERENCE.read_text(encoding="utf-8")
    assert "### Release-build flash savers" in reference
    assert "| `FL_LOG_LEVEL` 1 (errors only) |" in reference
    assert "## Artifacts" in reference
    assert "## Lessons baked in (do not re-discover)" in reference
