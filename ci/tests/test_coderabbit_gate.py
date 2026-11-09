from ci.coderabbit_gate import RATE_LIMIT_MARKER, Thread, decide, pr_from_ref


def _thread(resolved: bool, author: str = "coderabbitai") -> Thread:
    return Thread(author=author, resolved=resolved, path="a.h:1", url="u", title="t")


def test_unresolved_coderabbit_thread_fails() -> None:
    verdict = decide([_thread(False), _thread(True)], ["summary"])
    assert not verdict.ok
    assert len(verdict.open_threads) == 1


def test_all_resolved_passes() -> None:
    assert decide([_thread(True)], []).ok


def test_no_threads_passes() -> None:
    assert decide([], []).ok


def test_human_threads_do_not_block() -> None:
    assert decide([_thread(False, author="zackees")], []).ok


def test_rate_limited_coderabbit_passes() -> None:
    verdict = decide(
        [_thread(False)], [f"{RATE_LIMIT_MARKER}\n> ## Review limit reached"]
    )
    assert verdict.ok
    assert "rate limited" in verdict.reason


def test_pr_from_queue_ref() -> None:
    assert pr_from_ref("gh-readonly-queue/master/pr-5202-c96d28048002b41e89bb") == 5202
    assert pr_from_ref("refs/heads/gh-readonly-queue/master/pr-7-abc123") == 7
    assert pr_from_ref("master") is None
