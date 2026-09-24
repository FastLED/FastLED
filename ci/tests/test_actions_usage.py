"""Observed runner accounting stays explicit about missing event coverage."""

import json
import sys
from io import BytesIO
from unittest.mock import patch

import pytest

from ci.actions_usage import job_seconds, main, pages, summarize


class Response(BytesIO):
    def __init__(self, payload: dict) -> None:
        super().__init__(json.dumps(payload).encode())


def test_all_events_and_attempts_are_counted() -> None:
    runs = [
        {
            "id": 1,
            "name": "unit",
            "event": "pull_request",
            "run_attempt": 2,
            "conclusion": "success",
        },
        {
            "id": 2,
            "name": "board",
            "event": "pull_request",
            "run_attempt": 1,
            "conclusion": "failure",
        },
        {
            "id": 3,
            "name": "release",
            "event": "workflow_dispatch",
            "run_attempt": 1,
            "conclusion": "cancelled",
        },
    ]
    first = {
        "started_at": "2026-09-23T00:00:00Z",
        "completed_at": "2026-09-23T00:01:00Z",
    }
    second = {
        "started_at": "2026-09-23T00:02:00Z",
        "completed_at": "2026-09-23T00:02:30Z",
    }
    skipped = {"started_at": None, "completed_at": None}
    report = summarize(runs, {1: [first, second], 2: [first], 3: [skipped]})
    assert report["observed_runner_seconds"] == 150
    assert report["observed_runner_seconds_by_event"] == {
        "pull_request": 150,
        "workflow_dispatch": 0,
    }
    assert [run["run_id"] for run in report["runs"]] == [1, 2, 3]
    assert report["runs"][0] == {
        "run_id": 1,
        "workflow": "unit",
        "event": "pull_request",
        "latest_attempt": 2,
        "latest_conclusion": "success",
        "runner_seconds_all_attempts": 90,
    }
    assert job_seconds(skipped) == 0
    assert report["coverage_complete"] is False
    assert "workflow_run" in report["coverage_limits"][0]


def test_active_job_is_reported_instead_of_silently_counted_as_zero() -> None:
    runs = [
        {
            "id": 9,
            "name": "unit",
            "event": "push",
            "run_attempt": 1,
            "status": "in_progress",
        }
    ]
    job = {
        "id": 90,
        "status": "in_progress",
        "started_at": "2026-09-23T00:00:00Z",
        "completed_at": None,
    }
    report = summarize(runs, {9: [job]})
    assert report["active_run_ids"] == [9]
    assert report["active_job_ids"] == [90]
    assert report["coverage_complete"] is False


def test_main_normalizes_uppercase_sha(capsys: pytest.CaptureFixture[str]) -> None:
    lowercase_sha = "abcdef0123456789abcdef0123456789abcdef01"
    run = {
        "id": 7,
        "name": "unit",
        "event": "pull_request",
        "run_attempt": 1,
        "conclusion": "success",
        "head_sha": lowercase_sha,
    }
    responses = [
        Response({"total_count": 1, "workflow_runs": [run]}),
        Response({"total_count": 0, "jobs": []}),
    ]
    with (
        patch("ci.actions_usage.urlopen", side_effect=responses),
        patch("ci.actions_usage.os.environ", {"GITHUB_TOKEN": "token"}),
        patch.object(
            sys,
            "argv",
            ["actions_usage.py", "FastLED/FastLED", lowercase_sha.upper()],
        ),
    ):
        main()

    report = json.loads(capsys.readouterr().out)
    assert report["sha"] == lowercase_sha
    assert [run_detail["run_id"] for run_detail in report["runs"]] == [7]


def test_pagination_and_search_cap() -> None:
    first = {"total_count": 101, "workflow_runs": [{"id": i} for i in range(100)]}
    second = {"total_count": 101, "workflow_runs": [{"id": 100}]}
    with patch(
        "ci.actions_usage.urlopen", side_effect=[Response(first), Response(second)]
    ) as fetch:
        assert (
            len(pages("https://api.github.com/example", "token", max_results=1000))
            == 101
        )
        assert "page=2" in fetch.call_args_list[1].args[0].full_url

    capped = {"total_count": 1000, "workflow_runs": []}
    with patch("ci.actions_usage.urlopen", return_value=Response(capped)):
        with pytest.raises(ValueError, match="reached 1000"):
            pages("https://api.github.com/example", "token", max_results=1000)
