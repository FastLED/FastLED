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

## Validation

The official `espressif/idf:v5.5` image builds and links the esp32 application
without Arduino. The validated application binary is 240,368 bytes; the workflow
reports the size for each subsequent build and preserves its ELF/bin evidence.

The library recognizes `ESP_PLATFORM`, routes native IDF through the ESP32
dispatchers, uses toolchain-owned global size types, and declares its SDK
component dependencies. CPU frequency comes from SDK configuration when the
application does not supply `F_CPU`. The harness supplies no library workaround.
Hardware execution and other version/chip combinations remain unverified.
