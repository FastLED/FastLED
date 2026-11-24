"""Host-load scaling used by progress-aware CI deadlines (#5398)."""

import pytest

from ci.util import host_load


def test_load_scale_is_load_per_cpu(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(host_load.os, "getloadavg", lambda: (64.0, 0.0, 0.0))
    monkeypatch.setattr(host_load.os, "cpu_count", lambda: 16)
    assert host_load.load_scale() == pytest.approx(4.0)


def test_load_scale_never_shrinks_below_one(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(host_load.os, "getloadavg", lambda: (0.5, 0.0, 0.0))
    monkeypatch.setattr(host_load.os, "cpu_count", lambda: 16)
    assert host_load.load_scale() == 1.0


def test_load_scale_without_loadavg_is_one(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delattr(host_load.os, "getloadavg", raising=False)
    assert host_load.load_scale() == 1.0


def test_load_scale_is_capped(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(host_load.os, "getloadavg", lambda: (10000.0, 0.0, 0.0))
    monkeypatch.setattr(host_load.os, "cpu_count", lambda: 1)
    assert host_load.load_scale() == host_load.MAX_LOAD_SCALE
