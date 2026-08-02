# fbuild_qemu_smoke

Minimal standalone fbuild project used by `ci/tests/test_fbuild_qemu.py` to
verify that fbuild's native QEMU path (`fbuild test-emu`) can build and boot a
FastLED sketch on ESP32-QEMU. Its `platformio.ini` is fbuild's compatible
project manifest; PlatformIO is not invoked.

The sketch includes FastLED and a `WS2812` controller addition so a failure in
library resolution, compile, link, or early boot surfaces as a test failure.

Run manually:

```
fbuild test-emu tests/fbuild_qemu_smoke -e esp32dev --emulator qemu --timeout 10 \
  --halt-on-success 'FBUILD-QEMU-TEST-OK' \
  --halt-on-error 'Guru Meditation|abort\(\)|Backtrace:|TEST_SUITE_COMPLETE: FAIL|QEMU_LCD_CLOCKLESS_REGISTRATION: FAIL'
```

If the emulator prints `FBUILD-QEMU-TEST-OK` the path is working end-to-end.
