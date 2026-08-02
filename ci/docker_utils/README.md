# FastLED Docker Helpers

This directory hosts the **AVR8JS emulator** Docker helper only.

## What's here now

- **`Dockerfile.avr8js`** — AVR8JS-based emulator image used by
  `.github/workflows/avr8js_uno_test.yml` to run Arduino Uno / ATtiny firmware
  in JavaScript.
- **`avr8js/`** — Node packages + entrypoint scripts baked into the avr8js
  image.
- **`avr8js_docker.py`** — Python runner (`DockerAVR8jsRunner`) that pulls +
  invokes the avr8js image. Used from CI and from the local
  `ci.runners.avr8js_runner.run_avr8js_tests` entrypoint (`bash test --run uno`).
