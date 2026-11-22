"""The ./push delegate refuses a prooft older than 0.3.2, the first runner that pushes the attested SHA."""

from ci.prooft_push import parse_version, version_problem


def test_parse_version_reads_prooft_output() -> None:
    assert parse_version("prooft 0.3.2\n") == (0, 3, 2)
    assert parse_version("no version here") is None


def test_version_problem_accepts_the_runner_release_and_newer() -> None:
    assert version_problem("prooft 0.3.2") is None
    assert version_problem("prooft 0.10.1") is None
    assert version_problem("prooft 1.0.0") is None


def test_version_problem_rejects_older_or_unreadable() -> None:
    too_old = version_problem("prooft 0.3.1")
    assert too_old is not None and ">= 0.3.2" in too_old
    assert version_problem("garbage") is not None
