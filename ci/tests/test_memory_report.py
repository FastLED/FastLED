from ci.memory_report import MARKER, render


def test_render_marks_decreases_and_increases() -> None:
    base = {
        "uno": {"image_flash": 3600, "total_ram": 134},
        "esp32s3": {"image_flash": 326_023, "total_ram": 39_193},
    }
    head = {
        "uno": {"image_flash": 3660, "total_ram": 134},
        "esp32s3": {"image_flash": 320_655, "total_ram": 38_623},
    }
    out = render(base, head, "Blink")
    assert out.startswith(MARKER)
    assert "| uno | ⚠️ +60 | ⚠️ +1.67 | 0 | 0.00 |" in out
    assert "| esp32s3 | 💚 -5,368 | 💚 -1.65 | 💚 -570 | 💚 -1.45 |" in out


def test_render_abbreviates_large_deltas_and_lists_missing_boards() -> None:
    base = {
        "esp32c6": {"image_flash": 366_614, "total_ram": 19_544},
        "rp2040": {"image_flash": 1, "total_ram": 1},
    }
    head = {"esp32c6": {"image_flash": 348_182, "total_ram": 19_544}}
    out = render(base, head, "Blink")
    assert "💚 -18K" in out
    assert "`rp2040`" in out
