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

The local `components/fastled` entry includes the repository's CMake registration
with absolute source/include paths, so it also works when the checkout is not
named `FastLED`. It intentionally does not expose the root `idf_component.yml`:
that manifest supplies PlatformIO/pioarduino dependencies and uses rule syntax
unsupported by older component managers. Its dependencies remain unchanged.
Bare IDF uses the SDK dependencies declared in the shared CMake registration.
Configuration rejects Arduino
components, and the existing `FL_NO_ARDUINO` ratchet applies to all compiled
sources. The workflow requires both the linked ELF and a nonempty application
binary, reports the binary size and uploads build evidence.

The matrix in `.github/workflows/build_bare_idf.yml` covers IDF 5.5 with
esp32/esp32s3/esp32c3/esp32c6/esp32p4, plus IDF 5.1 and 4.4 with esp32/esp32s3.
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
Hardware execution remains unverified. See the PR's matrix results for evidence
for each version/chip pair.
