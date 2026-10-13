# fbuild_qemu_smoke

Minimal standalone fbuild project used by `ci/tests/test_fbuild_qemu.py` to
verify that fbuild's native QEMU path (`fbuild test-emu`) can build and boot a
FastLED sketch on ESP32-QEMU. Its `platformio.ini` is fbuild's compatible
project manifest.

The sketch includes FastLED and transmits one pixel through its existing
`MY9221` software GPIO driver, so a failure in library resolution, compile,
link, early boot, or `FastLED.show()` surfaces as a test failure. The pinned
[Espressif QEMU leaves ESP32 RMT unimplemented](https://github.com/espressif/qemu/blob/esp-develop-9.2.2-20250817/hw/xtensa/esp32.c#L529)
but models GPIO. Using the default WS2812 RMT driver previously waited for a
completion interrupt that could never arrive and could print success after a
ChannelManager timeout. Driver timeouts are now fatal.

This checks build/boot and software-driver execution. It does not validate
RMT completion or electrical waveform timing; those require hardware tests.

Run manually:

```
fbuild test-emu tests/fbuild_qemu_smoke -e esp32dev --emulator qemu --timeout 10 \
  --halt-on-success 'FBUILD-QEMU-TEST-OK' \
  --halt-on-error 'Guru Meditation|abort\(\)|Backtrace:|TEST_SUITE_COMPLETE: FAIL|QEMU_LCD_CLOCKLESS_REGISTRATION: FAIL|ChannelManager: Timeout'
```

If the emulator prints `FBUILD-QEMU-TEST-OK` the path is working end-to-end.
