from ci.wasm_test import _render_smoke_succeeded


def test_active_worker_without_frame_is_not_render_success() -> None:
    state = {"frameCount": 0, "controllerRunning": False, "workerActive": True}

    assert not _render_smoke_succeeded(state)


def test_rendered_frame_is_render_success() -> None:
    state = {"frameCount": 1, "controllerRunning": False, "workerActive": False}

    assert _render_smoke_succeeded(state)


def test_worker_frame_rendered_counts_toward_smoke_frame_count() -> None:
    """In worker mode FastLED_onFrame never runs on the main thread, so the
    worker manager must bump globalThis.fastLEDFrameCount itself (#4638)."""
    import re
    from pathlib import Path

    src = (
        Path(__file__).resolve().parents[2]
        / "src/platforms/wasm/compiler/modules/core/fastled_worker_manager.ts"
    ).read_text(encoding="utf-8")
    match = re.search(r"\n  handleFrameRendered\(payload\) \{(.*?)\n  \}\n", src, re.S)
    assert match, "handleFrameRendered not found"
    assert "globalThis.fastLEDFrameCount" in match.group(1)
