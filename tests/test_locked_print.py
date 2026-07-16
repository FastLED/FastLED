"""Tests for the CI logger's streaming behavior."""

import sys

from ci.util.locked_print import locked_print


class _RecordingStream:
    def __init__(self) -> None:
        self.writes: list[str] = []
        self.flush_count = 0

    def write(self, value: str) -> int:
        self.writes.append(value)
        return len(value)

    def flush(self) -> None:
        self.flush_count += 1


def test_locked_print_flushes_each_line(monkeypatch) -> None:
    stream = _RecordingStream()
    monkeypatch.setattr(sys, "stdout", stream)

    locked_print("first\nsecond")

    assert stream.writes == ["first", "\n", "second", "\n"]
    assert stream.flush_count == 2
