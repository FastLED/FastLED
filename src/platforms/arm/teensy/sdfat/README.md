# FastLED Teensy SdFat subset

This directory is a private, FAT32 read-only subset of Bill Greiman's
SdFat 2.1.2 as shipped with Teensyduino 1.60.  It is intentionally limited
to SPI card initialization, path lookup, and reads used by
`fl::FileSystem::beginSd()`.

The initial import retains every upstream copyright and MIT license notice.
After the verbatim import, FastLED changes will keep the implementation
private to this platform, remove framework-wide mutable state, and place any
necessary shared state behind `fl::Singleton` so an application that never
uses an SD card has no retained SdFat storage or constructors.

Excluded upstream features include exFAT, formatting, write support, SDIO,
iostream adapters, and Arduino-global `SD` compatibility APIs.
