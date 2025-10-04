# ARM Assembly Optimization for libhelix-mp3

The libhelix-mp3 decoder includes highly optimized ARM assembly implementations of the polyphase filter in `real/arm/asmpoly_gcc.S`.

## Automatic Selective Compilation

The assembly file is **always included** in the build, but uses preprocessor guards to compile **only on supported platforms**:

```c
#if defined(__GNUC__) && defined(__arm__) && !defined(__thumb__) && !defined(__thumb2__)
// ARM32 assembly code here
#endif
```

### ✅ Platforms Where Assembly IS Used
- **ARM Cortex-A** processors (Raspberry Pi with Linux, BeagleBone, etc.)
- **Legacy ARM7/ARM9** processors
- Any ARM platform supporting **full ARM32 instructions** (not just Thumb-2)

### ❌ Platforms Where Assembly is SKIPPED
- **ARM Cortex-M** processors (Teensy, Arduino ARM boards, RP2040)
  - Uses optimized C++ with Thumb-2 inline assembly instead
- **ESP32** (Xtensa architecture)
  - Uses Xtensa-optimized inline assembly instead
- **x86/AVR/other** platforms
  - Uses portable C++ implementations

## How It Works

1. **Build System**: `asmpoly_gcc.S` is always included via `library.json` srcFilter
2. **Preprocessor**: Guards detect platform capabilities at compile time
3. **ARM32 platforms**: Assembly code compiles, C++ polyphase is excluded
4. **Thumb-2 platforms**: Assembly code is empty, C++ polyphase compiles
5. **No user action needed**: Everything is automatic!

## Performance

| Platform Type | Implementation | Performance |
|--------------|----------------|-------------|
| **ARM Cortex-A** (Pi Linux) | ARM32 assembly | Fastest (~1-2ms/frame better) |
| **ARM Cortex-M** (Teensy) | C++ + Thumb-2 inline asm | Already optimized |
| **ESP32** (Xtensa) | C++ + Xtensa inline asm | Already optimized |
| **AVR/x86/Others** | Portable C++ | Good, compiler-optimized |

## Technical Details

### Assembly File
- `asmpoly_gcc.S` - GCC-compatible ARM32 assembly with preprocessor guards
- Compiles to empty object file on non-ARM32 platforms (no overhead)

### C++ Fallback (polyphase.cpp)
```cpp
#if !(defined(__GNUC__) && defined(__arm__) && !defined(__thumb__) && !defined(__thumb2__))
#define COMPILE_CPP_POLYPHASE  // Compile C++ version
#endif
```

### Function Naming
Assembly functions use `xmp3_` prefix (via `STATNAME` macro):
- `xmp3_PolyphaseMono`
- `xmp3_PolyphaseStereo`

These automatically match the C function declarations in `coder.h`.

### Removed Files
- `asmpoly.s` - ARM ADS assembler format (deleted, incompatible with GCC)
- `asmmisc.s` - ARM ADS assembler format (deleted, incompatible with GCC)

Only `asmpoly_gcc.S` (GCC format with preprocessor support) is kept.
