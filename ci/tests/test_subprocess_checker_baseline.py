"""Regression tests for the raw-subprocess ratchet's baseline handling.

The dangerous case is a narrow invocation: running the checker against a
single file with --update-baseline must not rewrite the baseline from just
that file's findings, because that would silently drop every other entry and
turn the ratchet off across the repo.
"""

from __future__ import annotations

from collections import Counter
from pathlib import Path

from ci.lint_python import subprocess_capture_checker as checker


PROJECT_ROOT = Path(__file__).resolve().parents[2]


def _write_baseline(path: Path, entries: dict[str, int]) -> None:
    checker.write_baseline(Counter(entries), path)


def test_narrow_update_keeps_unscanned_entries(tmp_path: Path) -> None:
    """--update-baseline on one file must preserve entries for other files."""
    scanned = tmp_path / "scanned.py"
    scanned.write_text(
        "import subprocess\n"
        "subprocess.run(['ls'], capture_output=True, encoding='utf-8')\n",
        encoding="utf-8",
    )

    baseline = tmp_path / "baseline.txt"
    _write_baseline(
        baseline,
        {
            "ci/other_file.py|SRC001": 7,
            "ci/another.py|SRC002": 3,
        },
    )

    rc = checker.main([str(scanned), "--baseline", str(baseline), "--update-baseline"])
    assert rc == 0

    result = checker.load_baseline(baseline)
    # The two untouched files must survive verbatim.
    assert result["ci/other_file.py|SRC001"] == 7
    assert result["ci/another.py|SRC002"] == 3


def test_narrow_scan_does_not_claim_unscanned_entries_improved(
    tmp_path: Path, capsys
) -> None:
    """A narrow scan must not report unexamined baseline entries as fixed."""
    scanned = tmp_path / "clean.py"
    scanned.write_text("x = 1\n", encoding="utf-8")

    baseline = tmp_path / "baseline.txt"
    _write_baseline(baseline, {"ci/elsewhere.py|SRC001": 5})

    rc = checker.main([str(scanned), "--baseline", str(baseline)])
    assert rc == 0

    out = capsys.readouterr().out
    # "now gone" would be a lie -- ci/elsewhere.py was never looked at.
    assert "now gone" not in out


def test_rescanned_file_can_still_shrink(tmp_path: Path) -> None:
    """A file that genuinely improved must have its count lowered, not frozen."""
    scanned = tmp_path / "fixed.py"
    scanned.write_text("x = 1\n", encoding="utf-8")  # no subprocess use left

    baseline = tmp_path / "baseline.txt"
    key = f"{checker._rel_path(scanned)}|SRC001"
    _write_baseline(baseline, {key: 4, "ci/untouched.py|SRC003": 2})

    rc = checker.main([str(scanned), "--baseline", str(baseline), "--update-baseline"])
    assert rc == 0

    result = checker.load_baseline(baseline)
    assert key not in result  # dropped to zero, so the entry goes away
    assert result["ci/untouched.py|SRC003"] == 2  # unrelated entry preserved


def test_new_violation_still_fails(tmp_path: Path) -> None:
    """The ratchet must still catch a genuinely new violation."""
    offender = tmp_path / "offender.py"
    offender.write_text(
        "import subprocess\nsubprocess.Popen(['ls'], stdout=subprocess.PIPE)\n",
        encoding="utf-8",
    )

    baseline = tmp_path / "baseline.txt"
    _write_baseline(baseline, {})

    rc = checker.main([str(offender), "--baseline", str(baseline)])
    assert rc == 1
