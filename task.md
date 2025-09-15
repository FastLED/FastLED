# ESP32 QEMU Testing - ROM File Challenge

## Current Status

The ESP32 QEMU testing infrastructure has been mostly fixed, but is blocked by a missing ROM file requirement.

## What Was Fixed

1. **Machine Type Error**: Changed "esp32dev" to "esp32" in `ci/qemu-esp32.py:130`
2. **Firmware Path Detection**: Updated both esptool and manual merge functions to correctly find firmware files in build directories
3. **esptool Integration**: Added esptool package and configured proper 4MB flash image creation with `--fill-flash-size` option
4. **Flash Image Size**: Now creates proper 4MB flash images (0x400000 bytes) as required by ESP32 QEMU

## Current Challenge

ESP32 QEMU emulation fails with:
```
.cache\qemu\qemu-system-xtensa.exe: Error: -bios argument not set, and ROM code binary not found (1)
```

The official Espressif QEMU (version 9.2.2 esp_develop_9.2.2_20250817) doesn't include built-in ROM code despite documentation suggesting it should.

## What Was Attempted

1. **Official ROM Binary**: Downloaded `esp32-v3-rom.bin` from https://github.com/espressif/qemu/raw/esp-develop/pc-bios/esp32-v3-rom.bin
2. **ROM ELF Files**: Attempted to download from https://github.com/espressif/esp-rom-elfs but releases returned "Not Found"
3. **Various QEMU Parameters**: Tried `-bios`, `-kernel`, and no ROM arguments
4. **Command Line Variations**: Tested different QEMU command configurations based on official documentation

## Required Solution

Need to either:
1. Find a working ESP32 ROM file (ELF format) that QEMU can load
2. Configure ESP32 QEMU to use built-in ROM (if available)
3. Use ESP-IDF's `idf.py qemu` approach instead of direct QEMU execution
4. Switch to a different ESP32 emulation approach

## Current Working Command

The fixed QEMU command that fails only due to ROM:
```bash
.cache\qemu\qemu-system-xtensa.exe -nographic -machine esp32 -drive file=.build\pio\esp32dev\.pio\build\esp32dev\flash.bin,if=mtd,format=raw -global driver=timer.esp32.timg,property=wdt_disable,value=true
```

## Files Modified

- `ci/qemu-esp32.py` - Fixed machine type and firmware paths
- `pyproject.toml` - Added esptool dependency

## Test Command

```bash
bash test --qemu esp32dev
```

All compilation works correctly. Only QEMU emulation is blocked by the ROM requirement.