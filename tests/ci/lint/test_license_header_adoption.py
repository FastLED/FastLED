"""Focused RED -> GREEN coverage for FastLED issue #4047."""

from pathlib import Path

from running_process import RunningProcess


REPO_ROOT = Path(__file__).parents[3]


def test_release_source_license_headers_are_canonical() -> None:
    result = RunningProcess.run(
        [
            "uv",
            "run",
            "tools/license_headers.py",
            "check",
            "--profile",
            "release",
            "--no-cache",
        ],
        cwd=str(REPO_ROOT),
        check=False,
        capture_output=True,
        text=True,
        timeout=120,
    )
    assert result.returncode == 0, (result.stdout or "") + (result.stderr or "")
