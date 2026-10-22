# Optional Arduino SPI provider on ESP32

Native ESP-IDF SPI remains the default. An application that already owns an
Arduino SPI bus can supply a strong `fl::platforms::fastled_spi_provider`
factory in one directly linked application translation unit. FastLED calls
this factory once per supported logical host, before acquiring native SPI.
A null result disables that host; it never falls back and takes the bus.
Logical hosts are 2 and 3, rather than Arduino or SDK host enum values.

## Application ownership

Include `platforms/arduino/spi/spi_arduino.hpp` and
`platforms/esp/32/drivers/spi/spi_provider.h` in the application. Begin an
application-owned `fl::shared_ptr<SPIClass>` with the desired pins, then create
an `fl::platforms::ArduinoSpiBus` with that owner, logical host, MOSI and SCK.
The supplied metadata must describe the actual configured bus; Arduino does
not expose public pin getters. The adapter never begins, ends or changes pins
on the owner's bus.

The factory returns `makeArduinoSpiHw1(owner, optionalSelectable)` for its
host and an empty pointer for hosts the application does not expose. Keep
the owner's setup ahead of the first FastLED SPI controller lookup. Factory
results, including null results, are cached for the process lifetime.
The application can use the existing legacy API with explicit bus selection:
`FastLED.addLeds<APA102, 4, 18, RGB, DATA_RATE_KHZ(500)>(leds, count, fl::Esp32SpiBus::HOST2)`.
Use that explicit host overload to select SPI_UNIFIED. Automatic engine selection
can choose a separate backend such as I2S_SPI; that backend does not use this
SpiHw1 provider.

Every cooperating client must use the same `ArduinoSpiBus`: `write()` holds
one mutex and one Arduino transaction over the entire buffer and optional
select/gating interval. `withBus()` accepts a synchronous callback for an
existing library operation, including reads; the callback manages its own
Arduino transactions and returns success. Close those transactions on error.
Do not recursively enter the guard, or end/reconfigure the raw SPIClass while
clients are active. Direct calls to SPIClass do not automatically share the
adapter's mutex.

APA102/SK9822 strips have no chip select. Sharing a lock prevents interleaved
transactions, but unrelated SPI traffic still clocks an ungated strip. Use
actual application wiring such as gating or a multiplexer when isolation is
required. An optional Selectable represents that wiring, not a chip select
invented for the LED chipset.

Arduino `writeBytes()` is synchronous and returns no partial-transfer status.
The adapter completes writes before returning, retains its staging buffer
until `waitComplete()`, and does not claim asynchronous DMA. Native SPI keeps
its asynchronous SDK implementation.

## Linkage and validation

The public factory declaration is strong; only the library's default definition
is weak. Link the application's override directly. An override present only
inside an otherwise unreferenced static archive is not extracted automatically.
The ELF regression tests exercise absent, archive-only and directly linked
providers against the actual selection implementation.

Run the staged fixtures through canonical AutoResearch on classic ESP32:

```sh
bash autoresearch esp32dev_idf44 --spi-provider --upload-port /dev/ttyUSB0
bash autoresearch esp32dev_idf44 --spi-provider --spi-provider-native --upload-port /dev/ttyUSB0
```

The Arduino fixture receives the complete legacy APA102 frame with the second
SPI peripheral and checks transaction settings and selection. The native
fixture injects SDK timeouts to check DMA lifetime and polling recovery.
These fixtures temporarily occupy SPI2/SPI3 and GPIO4/18/19/21. Restore the
ordinary AutoResearch image after the checks. SDK fault aliases are confined
to the staged test build.

## Matched footprint (#4890)

Lolin D32, espressif32@6.13.0, default generated flags; baseline source
47ad6f04. Both clocked cases use legacy APA102 HOST2, GPIO4/18, 500 kHz,
32 pixels. Physical static RAM is the fbuild board total.

| Case | Flash before | Flash after | Static RAM before | Static RAM after |
|---|---:|---:|---:|---:|
| No SPI | 231094 | 231094 | 21000 | 21000 |
| Native SPI | 304118 | 304170 | 23040 | 23040 |
| Application Arduino SPI owner | 308746 | 291922 | 23128 | 22796 |

The explicit provider removes 16824 flash bytes and 332 static RAM bytes by
letting the linker discard native SDK SPI master acquisition, transaction
validation, queue/DMA and bus-lock code. Arduino SPI remains linked for the
application. The baseline Arduino-owner sketch retains the same application
SPIClass setup, but lacks working FastLED sharing; these are footprint controls,
not two working sharing implementations. The native path adds 52 flash bytes
for selection and verified DMA timeout/allocation handling, with unchanged
static RAM. No-SPI applications remain byte-identical.

An explicit provider also compiles and links on ESP32-P4 with Arduino3.3.11 /
IDF5.5.5. This is API compatibility evidence, not physical P4 SPI validation.

### Device validation

On classic ESP32 / IDF4.4.1, both staged modes pass cold and warm checks.
The provider fixture validates all 140 APA102 frame bytes with zero mismatches,
actual 500 kHz / mode 0 / MSB settings, selection levels, and a cooperating
257-byte second client using 1 MHz / mode 3 / LSB. A test-only SDK alias
observes the LED operation attempting the exact shared mutex while that client
holds it, avoiding a timing-only contention assumption. Contended show times
were about 6.2 ms, including the peer transaction and scheduling; this is not
a standalone SPI throughput benchmark.

Native SDK timeout injection reproduces the old premature buffer release.
The fixed driver retains storage, retrieves the later result, recovers through
polling and accepts another transfer. An injected DMA allocation failure also
reproduces an incorrectly usable buffer; the driver now checks actual storage
size and clears readiness on failure.

The first hardware lifecycles include lazy driver/task allocations; the harness
prints those measurements and requires exact heap parity on two later cycles.
Both provider warm cycles measured 112724 -> 112724 free bytes; the native
warm cycle measured 117016 -> 117016. These fixtures exercise different
workloads, so their absolute free-heap values are not a matched comparison.
Response JSON allocation is outside each measured interval. The Arduino
fixture's interval starts after application bus initialization and includes
creation and release of temporary clients and the capture/peer task lifecycle.
