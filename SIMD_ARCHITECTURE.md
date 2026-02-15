# FastLED SIMD Architecture Guide

## What is AVX2?

**AVX2 (Advanced Vector Extensions 2)** is an x86/x64 SIMD instruction set extension:
- **Platform**: Intel/AMD CPUs only (Core i3/i5/i7 since ~2013, Ryzen since 2017)
- **Vector width**: 256 bits (8×32-bit integers or 4×64-bit floats)
- **Key features**:
  - Gather instructions (`_mm256_i32gather_epi32`) - perfect for permutation table lookups!
  - Parallel arithmetic on 8 values simultaneously
  - Available on: Desktop/laptop x86 CPUs, NOT on embedded ARM/Xtensa/RISC-V

**Bottom line**: AVX2 is **x86-only** and does **NOT** help ESP32-S3.

---

## FastLED's SIMD Abstraction Layer

FastLED has a **platform-agnostic SIMD abstraction** that dispatches to different implementations:

```
fl::simd::
  ├── platforms::x86        → SSE2/SSE4.1 (NO AVX2 yet!)
  ├── platforms::xtensa     → Xtensa PIE (ESP32, ESP32-S3) - SCALAR FALLBACK
  ├── platforms::riscv      → RISC-V V extension (ESP32-C6) - SCALAR FALLBACK
  ├── platforms::arm_neon   → ARM NEON (Teensy 4.x, RP2040, STM32)
  └── platforms::noop       → Scalar fallback (AVR, ESP8266, etc.)
```

### Platform-Specific Files

| File | Platform | SIMD ISA | Current Status |
|------|----------|----------|----------------|
| `simd_x86.hpp` | Intel/AMD x64 | **SSE2/SSE4.1** | ✅ Fully implemented |
| `simd_xtensa.hpp` | ESP32/S2/S3 | Xtensa PIE | ⚠️ **Scalar fallback** |
| `simd_riscv.hpp` | ESP32-C6 | RISC-V V | ⚠️ **Scalar fallback** |
| `simd_arm_neon.hpp` | ARM Cortex-A/M | ARM NEON | ✅ Fully implemented |
| `simd_noop.hpp` | AVR, 8266, etc. | None | Scalar only |

---

## SIMD on ESP32-S3

### ESP32-S3 Architecture
- **CPU**: Dual-core Xtensa LX7 @ 240 MHz
- **SIMD Extension**: **Xtensa PIE** (Processor Interface Extension)
- **Vector width**: 128 bits (4×32-bit integers)
- **Status in FastLED**: ⚠️ **NOT YET IMPLEMENTED** (scalar fallback only)

### Xtensa PIE Instructions (Available but Unused)

ESP32-S3 has PIE SIMD instructions that FastLED could use:
```c
// Vector load/store
ee.vld.128.ip    // Load 128-bit vector
ee.vst.128.ip    // Store 128-bit vector

// Integer arithmetic
ee.vadd.s32      // Add 4×i32
ee.vmul.s32      // Multiply 4×i32
ee.vmin.s32      // Min 4×i32
ee.vmax.s32      // Max 4×i32

// Bitwise operations
ee.vand          // AND vectors
ee.vor           // OR vectors
ee.vxor          // XOR vectors
```

**Current FastLED code** (from `simd_xtensa.hpp:58-64`):
```cpp
// PIE-ready: Can be replaced with ee.vld.128.ip / ee.vldbc.8
simd_u8x16 result;
for (int i = 0; i < 16; ++i) {
    result.data[i] = ptr[i];  // SCALAR LOOP! No PIE instructions yet
}
return result;
```

---

## Current SIMD Status in Perlin Noise

### What the callgrind profile was testing

The callgrind test ran on **x86 (Windows/Linux host)**, which uses `simd_x86.hpp`:
- ✅ Uses SSE2 intrinsics (`_mm_loadu_si128`, `_mm_add_epi32`, etc.)
- ❌ **NO AVX2** - FastLED doesn't have AVX2 support yet
- ❌ **NO gather instructions** - Permutation lookups are still scalar

### When you run on ESP32-S3

The same Perlin code on ESP32-S3 uses `simd_xtensa.hpp`:
- ❌ **Scalar fallback** - No PIE instructions implemented
- ❌ Loops over arrays instead of vector ops
- ❌ Much slower than it could be

---

## Performance Matrix: Platform-Specific Speedup Potential

| Platform | Current SIMD | Perlin Speedup from True SIMD |
|----------|--------------|-------------------------------|
| **x86 (SSE2)** | ✅ Enabled | **1.5-2× with AVX2 gather** |
| **ESP32-S3 (Xtensa PIE)** | ❌ Scalar fallback | **2-3× with PIE implementation** |
| **ESP32-C6 (RISC-V V)** | ❌ Scalar fallback | **2-3× with RVV implementation** |
| **Teensy 4.x (ARM NEON)** | ✅ Enabled | Already optimized |
| **AVR/ESP8266** | N/A (no SIMD) | None |

---

## Optimization Strategy: Platform-Specific

### For x86 (callgrind profiling, host testing)

**Current**: Uses SSE2 (128-bit, 4×i32)
**Optimization**: Add AVX2 support (256-bit, 8×i32)

```cpp
// Current SSE2 (simd_x86.hpp already has this):
__m128i load_u32_4(const u32* ptr) {
    return _mm_loadu_si128((__m128i*)ptr);  // Load 4×u32
}

// Proposed AVX2 (NEW):
__m256i load_u32_8(const u32* ptr) {
    return _mm256_loadu_si256((__m256i*)ptr);  // Load 8×u32
}

// AVX2 gather for permutation lookups (CRITICAL for Perlin):
__m128i gather_u8_4(const u8* base, __m128i indices) {
    // Gather 4 bytes from random locations in one instruction!
    return _mm_i32gather_epi32((int*)base, indices, 1);
}
```

**Expected speedup**: 1.5-2× on x86 hosts

---

### For ESP32-S3 (Xtensa PIE)

**Current**: Scalar fallback (loops)
**Optimization**: Implement Xtensa PIE intrinsics

```cpp
// Current scalar (simd_xtensa.hpp):
simd_u32x4 load_u32_4(const u32* ptr) {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];  // Scalar loop
    }
    return result;
}

// Proposed PIE implementation (NEW):
simd_u32x4 load_u32_4(const u32* ptr) {
    simd_u32x4 result;
    asm volatile (
        "ee.vld.128.ip %0, %1, 16"  // Load 128-bit vector (4×u32)
        : "=r"(result)
        : "r"(ptr)
    );
    return result;
}
```

**Expected speedup**: 2-3× on ESP32-S3

---

## Why "SIMD" is in the Name but Not Used

The current Perlin implementation is called `pnoise2d_raw_simd4`, but it's **batched scalar**, not true SIMD:

```cpp
// Current implementation (animartrix2_detail.hpp:157-206):
static inline void pnoise2d_raw_simd4(
    const fl::i32 nx[4], const fl::i32 ny[4],
    const fl::i32 *fade_lut, const fl::u8 *perm,
    fl::i32 out[4])
{
    // Process 4 pixels, but using scalar loops:
    for (int i = 0; i < 4; i++) {
        AA[i] = perm[A[i] & 255];  // Scalar load, not vector load!
    }
}
```

**Why it's named "simd4"**: It processes 4 values per call (SIMD-ready interface)
**Why it's not SIMD**: The implementation uses loops, not vector instructions

---

## Action Plan: Platform-Specific Optimizations

### Priority 1: x86 AVX2 (for callgrind profiling)

**Goal**: Speed up Perlin on development machines (Windows/Linux/Mac)
**Benefit**: Faster iteration during development, better profiling

**Steps**:
1. Add AVX2 feature detection to `simd_x86.hpp`
2. Implement 8-wide operations (`load_u32_8`, `add_u32_8`, etc.)
3. Add AVX2 gather for permutation lookups
4. Update Perlin to use 8-wide batches when AVX2 available

**Expected speedup**: 1.5-2× on x86 hosts (callgrind testing)

---

### Priority 2: ESP32-S3 Xtensa PIE (for production)

**Goal**: Speed up Perlin on actual hardware (ESP32-S3 LEDs)
**Benefit**: Faster animation rendering on deployed devices

**Steps**:
1. Research Xtensa PIE instruction set (ESP-IDF docs)
2. Implement PIE intrinsics in `simd_xtensa.hpp`
3. Test on ESP32-S3 hardware
4. Benchmark improvement

**Expected speedup**: 2-3× on ESP32-S3

---

### Priority 3: ESP32-C6 RISC-V V extension (future-proofing)

**Goal**: Support next-gen ESP32 chips (C6, P4, H2)
**Benefit**: Prepares for upcoming RISC-V-based ESP32 variants

**Expected speedup**: 2-3× on ESP32-C6/P4/H2

---

## Comparison: AVX2 vs. Xtensa PIE

| Feature | AVX2 (x86) | Xtensa PIE (ESP32-S3) |
|---------|------------|------------------------|
| **Vector width** | 256-bit (8×32-bit) | 128-bit (4×32-bit) |
| **Gather support** | ✅ `_mm256_i32gather_epi32` | ❌ No gather (scalar fallback) |
| **Platforms** | Intel/AMD x64 | ESP32, ESP32-S2, ESP32-S3 |
| **FastLED status** | ❌ Not implemented | ❌ Not implemented |
| **Development priority** | High (faster testing) | **Critical (production)** |

---

## Recommendation for ESP32-S3 Users

**If you care about ESP32-S3 performance**, the optimization priority should be:

1. ✅ **Cache alignment** (DONE, +6.2%)
2. **Switch to perlin_q16** (+30-50%, works on all platforms)
3. **Implement Xtensa PIE** (+200-300% on ESP32-S3 only)
4. Add prefetch hints (+10-20%, works on all platforms)
5. Simplex noise (+500-900%, works on all platforms)

**AVX2 only helps on x86 hosts** (development machines), not ESP32-S3 hardware.

---

## Conclusion

**AVX2** = Intel/AMD x86 SIMD (NOT for ESP32-S3)
**Xtensa PIE** = ESP32-S3 SIMD (NOT YET IMPLEMENTED in FastLED)

Both would help Perlin performance, but on **different platforms**:
- AVX2: Faster development/testing on x86 hosts
- Xtensa PIE: Faster LED animations on ESP32-S3 devices

**Next steps**: Choose whether to optimize for x86 testing (AVX2) or ESP32-S3 production (Xtensa PIE).
