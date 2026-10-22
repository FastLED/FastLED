# ESP32 embedded storage

`fl::getEmbeddedFs()` uses the SDK LittleFS VFS on ESP32. The public
`fl::FileSystem` API stays the same. Storage remains in a separate translation
unit, so applications that never request it do not retain either backend.

## Mount ownership

The native backend uses partition label `spiffs` and VFS route `/littlefs`.
It mounts on `begin()` and unmounts after the final filesystem or open file
releases its lease. Calling `end()` on a filesystem does not invalidate its
already open files. Repeated `begin()` calls are idempotent.

If the partition is already mounted, FastLED borrows it and never formats or
unmounts it. That owner must use `/littlefs` and remain mounted until all
borrowers finish. Older SDKs cannot report a borrowed volume's route; checking
that `/littlefs` is a directory cannot establish its volume identity if an
unrelated VFS occupies that path. Use the explicit Arduino adapter for custom
Arduino routes. External remounts while files are open are unsupported.

Formatting defaults to false. `getEmbeddedFs(true)` permits formatting only
when the native backend mounts the partition itself. Reads and metadata follow
the SDK's semantics: an existing descriptor may retain cached metadata and
reach EOF after another descriptor appends. Reopen the file to observe changes.

## Optional Arduino provider

Applications already using Arduino LittleFS can provide this factory in a
directly linked application translation unit:

```cpp
#include <LittleFS.h>
#include "platforms/embedded_fs_provider.h"
#include "platforms/arduino/fs/embedded_fs_arduino.hpp"

namespace fl { namespace platforms {
FsImplPtr fastled_embedded_fs_provider(bool formatOnFail) FL_NO_EXCEPT {
    return makeArduinoEmbeddedFs(formatOnFail);
}
}}
```

The application calls `LittleFS.begin(...)` before beginning its FastLED
filesystem. It handles formatting and calls `LittleFS.end()` only after all
adapters and their open files finish. The ESP32 adapter only borrows Arduino's
object; it does not mount, format or unmount it. This also supports an
application-selected Arduino mount route.

The strong factory replaces FastLED's weak default at link time, allowing the
native backend to be discarded. A provider returning null reports failure;
FastLED does not silently mount a fallback. An optional provider sitting only
in an otherwise unreferenced archive is not automatically extracted. Do not
make the application definition weak.

## Reproducible validation

The linker tests exercise the actual production entry point with no provider,
an archive-only provider and a directly linked provider. Run them with:

```sh
bash test --py embedded_fs
```

Physical checks stage a fixture into the canonical AutoResearch project:

```sh
bash autoresearch esp32dev_idf44 --embedded-fs --upload-port /dev/ttyUSB0 --timeout 60s
bash autoresearch esp32dev_idf44 --embedded-fs --embedded-fs-provider --upload-port /dev/ttyUSB0 --timeout 60s
```

These commands create/replace two reserved `fastled_4889*.bin` fixture files.
They refuse an already mounted external owner. They do not format by default;
`--embedded-fs-format-fixture` explicitly permits formatting the test partition
if it cannot mount. Use a test device with no valuable data on that partition.
Each run checks cold and warm lifecycles, byte contents, seeking, EOF, errors,
multiple filesystems, file lifetime, borrowed mounts and custom routes. The
host verifies the selected backend and requires zero retained heap in warm
cycles. Native reads are also compared with actual SDK descriptors.

SDK compatibility has been compiled on Arduino 2 / IDF 4.4 and Arduino 3 /
IDF 5.5. Physical classic ESP32 evidence does not establish USB behavior on
other ESP32 variants; that is tracked separately in #4893.

## Size comparison for #4889

Matched Lolin D32 builds use `espressif32@6.13.0` (Arduino 2.0.17,
IDF 4.4.7), the same partition/configuration and the same read/seek/lifetime
probe. Baseline is `b28cb3bd988aeef163818973ba108830c52a3cc3` (#4887).
Flash and static RAM below are fbuild build totals; `bash bloat` verifies
the retained symbols. Symbol-attributed RAM includes SDK aliases and is not
the physical static RAM total.

| Sketch | Before flash | After flash | Change | Before static RAM | After static RAM |
| --- | ---: | ---: | ---: | ---: | ---: |
| No storage calls | 231,094 | 231,094 | 0 | 21,000 | 21,000 |
| Default embedded storage | 293,858 | 284,070 | -9,788 | 21,420 | 21,412 |
| Application-mounted Arduino provider | 293,894 | 293,998 | +104 | 21,420 | 21,420 |

The default eliminates Arduino `String` and `fs::File` symbols. An explicit
Arduino provider retains those expected symbols and eliminates
`NativeEmbeddedFile` entirely. The provider comparison includes the same real
application `LittleFS.begin()`/`end()` lifecycle on both sides.

On the physical classic ESP32 / Arduino 2.0.3 / IDF 4.4.1, the native fixture
passes 105 checks per invocation. Its first call retains 96 bytes for lazy
mount synchronization; warm cycles retain zero bytes. The Arduino-provider
fixture passes 82 checks per invocation, including eight factory calls on
the cold run and sixteen cumulatively on the warm run. Its first cycle
retains 440 bytes for lazy SDK allocations; warm cycles retain zero bytes.
These runtime heap costs are separate from the static RAM table.
