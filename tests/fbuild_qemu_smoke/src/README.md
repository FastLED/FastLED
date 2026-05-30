# src/

FastLED-dependent sketch (`main.cpp`) for the fbuild native-QEMU smoke test.
Uses the `.cpp` extension (rather than `.ino`) so the Arduino library lint
doesn't flag it as a stray sketch outside `examples/` (rule LD003). PlatformIO
compiles both extensions identically when they sit in `src/`.
Prints `FBUILD-QEMU-TEST-OK` on successful boot so the test runner can detect
a clean emulator startup.
