# Bare ESP-IDF link smoke test

This is a pure ESP-IDF application: `app_main()` registers three WS2812 LEDs
on GPIO 2, sets red/green/blue, calls `FastLED.show()`, prints one line and
returns. The LED array has static storage because FastLED retains it.
No Arduino component or sketch runner is used.

From the repository root, with Docker available:

```sh
docker run --rm -v "$PWD:/project" -w /project espressif/idf:v5.5 \
  idf.py -C ci/idf_app set-target esp32 build
stat -c '%s bytes' ci/idf_app/build/fastled_bare_idf.bin
```

With an activated ESP-IDF installation, the equivalent is:

```sh
idf.py -C ci/idf_app set-target esp32 build
```

`EXTRA_COMPONENT_DIRS` registers the repository itself. The main component
computes its dependency name from the checkout directory, so it also works
when the checkout is not named `FastLED`. Configuration rejects Arduino
components, and the existing `FL_NO_ARDUINO` ratchet applies to all compiled
sources. The workflow requires both the linked ELF and a nonempty application
binary, reports the binary size and uploads build evidence.

The matrix starts with IDF 5.5 / esp32. Each additional version/chip pair is
one `include` entry in `.github/workflows/build_bare_idf.yml`; IDF 4.4, 5.1,
6.x and esp32s3/c3/c6/p4 are future coverage, not claimed support here.
The PR gate uses the existing `ci-platform:esp32dev_idf5_component` label
(and the usual `ci-platform:esp*` / `ci-full` labels).

A successful build proves configuration, compilation and linking. It does
not prove GPIO output or runtime behavior on hardware; the printed message
is only observed if the application is run on a device.

## Current validation blocker

The first real IDF 5.5 / esp32 build configured successfully without Arduino,
but failed during FastLED compilation before linking. In translation units
that have not yet included `sdkconfig.h`, `src/platforms/esp/is_esp.h` checks
`ESP32`, `ARDUINO_ARCH_ESP32` and `CONFIG_IDF_TARGET_*`, while the IDF compiler
command supplies `ESP_PLATFORM` and does not predefine those target macros.
FastLED then reports an unrecognized platform and attempts to include
`avr/interrupt.h`; other units also report conflicting integer/size typedefs.
The harness deliberately supplies no workaround for this library blocker.
No successful link, application binary size or hardware result is claimed.
