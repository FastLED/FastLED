# ESP32 console integration with stock frameworks

This integration combines the string/RPC allocation correction, autonomous
legacy RMT4 output, and native SDK console implementation from #4885/#4887.
It uses unmodified Arduino-ESP32 and ESP-IDF packages. No framework fork,
patched SDK archive, or #4898 framework startup hook is required.

## Console ownership and compatibility

Including FastLED in a stock Arduino build does not require changing global
USB startup flags. When Arduino owns CDC (`ARDUINO_USB_CDC_ON_BOOT=1`),
FastLED's default console uses the SDK UART. Application `Serial` continues
using the framework's USB transport. Consequently, FastLED logs/RPC and
application USB Serial are different routes in that configuration; this does
not preserve an implicit USB route for FastLED's own console.

Native USB is selected explicitly with the existing
`FASTLED_ESP_FORCE_USB_SERIAL_JTAG` or `FL_ESP_USE_TINYUSB_CDC` option.
A conflicting framework USB owner is rejected only when native USB is
requested. Native TinyUSB requires all framework automatic USB startup
options disabled and supports CDC only. It does not provide Arduino's
composite classes or software upload/reset handshake.

Existing FBuild/AutoResearch USB profiles intentionally select a native USB
console and configure exclusive ownership through supported stock-framework
build flags. Those profiles retain their upload-port RPC route. QEMU selects
UART. These project configurations do not impose USB flag changes on other
Arduino applications merely to include FastLED.

The optional application writer hook can explicitly route complete FastLED
writes through an existing application stream; it redirects output only.
Input and initialization remain on the selected native transport.

## Costs and evidence limits

Historical matched measurements in #4885 report autonomous legacy RMT4 costs
of 2,700 flash bytes and 8 static RAM bytes compared with its blocking control,
and 3,040 additional retained heap bytes in the separate large-frame device
harness. Historical #4887 console measurements report savings of 3,560 flash
bytes and 240 static RAM bytes against that async baseline. These are original
matched controls, not fresh measurements of the integrated master revision.
Do not sum different workload/accounting measurements into a new size claim.

Stock-framework startup can still retain `Serial0`/HardwareSerial independently
of FastLED. This integration does not promise their elimination and does not
install the custom startup patch proposed by #4898.

The integration must pass current native sanitizer tests, stock-SDK compile
checks, and full CI before merging. Compile/link evidence does not establish
physical USB enumeration, reconnect, heap, or composite-device behavior.
The remaining physical USB acceptance work is tracked in #4893.
