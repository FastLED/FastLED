from ci.wasm_test import _render_smoke_succeeded


def test_active_worker_without_frame_is_not_render_success() -> None:
    state = {"frameCount": 0, "controllerRunning": False, "workerActive": True}

    assert not _render_smoke_succeeded(state)


def test_rendered_frame_is_render_success() -> None:
    state = {"frameCount": 1, "controllerRunning": False, "workerActive": False}

    assert _render_smoke_succeeded(state)
