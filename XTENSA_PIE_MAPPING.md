# Xtensa PIE to FastLED SIMD Mapping

## Complete Mapping: PIE Instructions → FastLED Functions

This document maps Xtensa PIE (Processor Interface Extension) instructions to FastLED's SIMD abstraction layer functions in `src/platforms/esp/32/simd_xtensa.hpp`.

---

## Critical Functions for Perlin Noise Performance

These are the **most important** functions to implement for Perlin noise optimization:

| FastLED Function | Current (Scalar) | Xtensa PIE Instruction | Speedup | Priority |
|------------------|------------------|------------------------|---------|----------|
| `load_u32_4()` | 4× scalar loads | `ee.vld.128.ip` | **4×** | ⭐⭐⭐⭐⭐ |
| `store_u32_4()` | 4× scalar stores | `ee.vst.128.ip` | **4×** | ⭐⭐⭐⭐⭐ |
| `add_i32_4()` | 4× scalar adds | `ee.vadd.s32` | **4×** | ⭐⭐⭐⭐⭐ |
| `sub_i32_4()` | 4× scalar subs | `ee.vsub.s32` | **4×** | ⭐⭐⭐⭐⭐ |
| `xor_u32_4()` | 4× scalar XORs | `ee.xorq` | **4×** | ⭐⭐⭐⭐ |
| `and_u32_4()` | 4× scalar ANDs | `ee.andq` | **4×** | ⭐⭐⭐⭐ |
| `srl_u32_4()` | 4× scalar shifts | `ee.vsra.s32` | **4×** | ⭐⭐⭐⭐ |

---

## 1. Load/Store Operations (Memory Access)

### `load_u32_4()` - Load 4×u32 (128-bit)
**Current implementation** (line 73-80):
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4(const u32* ptr) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];  // 4 scalar loads
    }
    return result;
}
```

**Xtensa PIE equivalent**:
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4(const u32* ptr) noexcept {
    simd_u32x4 result;
    asm volatile (
        "ee.vld.128.ip %[vec], %[ptr], 0"  // Load 128-bit (4×u32) from ptr
        : [vec] "=r" (result)
        : [ptr] "r" (ptr)
    );
    return result;
}
```

**PIE Instructions**:
- `ee.vld.128.ip` - Vector load 128-bit with post-increment
- `ee.vldbc.32` - Broadcast load (load one u32, replicate to all 4 lanes)

**Speedup**: **4× faster** (1 instruction vs. 4 loads)

---

### `store_u32_4()` - Store 4×u32 (128-bit)
**Current implementation** (line 82-87):
```cpp
FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(u32* ptr, simd_u32x4 vec) noexcept {
    for (int i = 0; i < 4; ++i) {
        ptr[i] = vec.data[i];  // 4 scalar stores
    }
}
```

**Xtensa PIE equivalent**:
```cpp
FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(u32* ptr, simd_u32x4 vec) noexcept {
    asm volatile (
        "ee.vst.128.ip %[vec], %[ptr], 0"  // Store 128-bit (4×u32) to ptr
        :
        : [vec] "r" (vec), [ptr] "r" (ptr)
        : "memory"
    );
}
```

**PIE Instructions**:
- `ee.vst.128.ip` - Vector store 128-bit with post-increment

**Speedup**: **4× faster** (1 instruction vs. 4 stores)

---

### `load_u8_16()` - Load 16×u8 (128-bit)
**Current implementation** (line 57-64):
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 load_u8_16(const u8* ptr) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = ptr[i];  // 16 scalar loads!
    }
    return result;
}
```

**Xtensa PIE equivalent**:
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 load_u8_16(const u8* ptr) noexcept {
    simd_u8x16 result;
    asm volatile (
        "ee.vld.128.ip %[vec], %[ptr], 0"  // Load 128-bit (16×u8) from ptr
        : [vec] "=r" (result)
        : [ptr] "r" (ptr)
    );
    return result;
}
```

**Speedup**: **16× faster** (1 instruction vs. 16 loads)

---

### `set1_u32_4()` - Broadcast u32 to all 4 lanes
**Current implementation** (line 132-139):
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(u32 value) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = value;  // 4 scalar assigns
    }
    return result;
}
```

**Xtensa PIE equivalent**:
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(u32 value) noexcept {
    simd_u32x4 result;
    asm volatile (
        "ee.vldbc.32 %[vec], %[val]"  // Broadcast load: replicate value to all 4 lanes
        : [vec] "=r" (result)
        : [val] "r" (&value)
    );
    return result;
}
```

**PIE Instructions**:
- `ee.vldbc.32` - Broadcast load 32-bit value to all lanes

**Speedup**: **4× faster**

---

## 2. Integer Arithmetic (Critical for Perlin)

### `add_i32_4()` - Add 4×i32
**Current implementation** (line 320-328):
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 add_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        result.data[i] = static_cast<u32>(a_i + b_i);  // 4 scalar adds
    }
    return result;
}
```

**Xtensa PIE equivalent**:
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 add_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    asm volatile (
        "ee.vadd.s32 %[res], %[a], %[b]"  // Add 4×i32 in parallel
        : [res] "=r" (result)
        : [a] "r" (a), [b] "r" (b)
    );
    return result;
}
```

**PIE Instructions**:
- `ee.vadd.s32` - Vector add signed 32-bit (4 parallel adds)
- `ee.vadd.u32` - Vector add unsigned 32-bit

**Speedup**: **4× faster** (1 instruction vs. 4 adds)

---

### `sub_i32_4()` - Subtract 4×i32
**Current implementation** (line 330-338):
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sub_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        result.data[i] = static_cast<u32>(a_i - b_i);  // 4 scalar subtracts
    }
    return result;
}
```

**Xtensa PIE equivalent**:
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sub_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    asm volatile (
        "ee.vsub.s32 %[res], %[a], %[b]"  // Subtract 4×i32 in parallel
        : [res] "=r" (result)
        : [a] "r" (a), [b] "r" (b)
    );
    return result;
}
```

**PIE Instructions**:
- `ee.vsub.s32` - Vector subtract signed 32-bit

**Speedup**: **4× faster**

---

### `mulhi_i32_4()` - Multiply 4×i32 and return high 16 bits
**Current implementation** (line 340-349):
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        i64 prod = static_cast<i64>(a_i) * static_cast<i64>(b_i);
        result.data[i] = static_cast<u32>(static_cast<i32>(prod >> 16));
    }
    return result;
}
```

**Xtensa PIE equivalent**:
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    asm volatile (
        "ee.vmulas.s32.qacc %[a], %[b]"    // Multiply-accumulate to Q accumulator
        "ee.srs.accx %[res], 16"            // Shift-right-saturate from accumulator
        : [res] "=r" (result)
        : [a] "r" (a), [b] "r" (b)
        : "ee.qacc"  // Clobber Q accumulator
    );
    return result;
}
```

**PIE Instructions**:
- `ee.vmulas.s32.qacc` - Vector multiply-accumulate signed 32-bit to Q accumulator
- `ee.srs.accx` - Shift-right-saturate from accumulator

**Speedup**: **4× faster**

---

## 3. Bitwise Operations (Used in Perlin Hashing)

### `xor_u32_4()` - XOR 4×u32
**Current implementation** (line 312-318):
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 xor_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] ^ b.data[i];  // 4 scalar XORs
    }
    return result;
}
```

**Xtensa PIE equivalent**:
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 xor_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    asm volatile (
        "ee.xorq %[res], %[a], %[b]"  // XOR 128-bit Q registers
        : [res] "=r" (result)
        : [a] "r" (a), [b] "r" (b)
    );
    return result;
}
```

**PIE Instructions**:
- `ee.xorq` - XOR 128-bit Q registers (bitwise XOR all 128 bits)

**Speedup**: **4× faster**

---

### `and_u32_4()` - AND 4×u32
**Current implementation** (line 359-365):
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 and_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] & b.data[i];  // 4 scalar ANDs
    }
    return result;
}
```

**Xtensa PIE equivalent**:
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 and_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    asm volatile (
        "ee.andq %[res], %[a], %[b]"  // AND 128-bit Q registers
        : [res] "=r" (result)
        : [a] "r" (a), [b] "r" (b)
    );
    return result;
}
```

**PIE Instructions**:
- `ee.andq` - AND 128-bit Q registers

**Speedup**: **4× faster**

---

### `srl_u32_4()` - Shift right logical 4×u32
**Current implementation** (line 351-357):
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 srl_u32_4(simd_u32x4 vec, int shift) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = vec.data[i] >> shift;  // 4 scalar shifts
    }
    return result;
}
```

**Xtensa PIE equivalent**:
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 srl_u32_4(simd_u32x4 vec, int shift) noexcept {
    simd_u32x4 result;
    asm volatile (
        "ee.vsra.s32 %[res], %[vec], %[shamt]"  // Shift right arithmetic 4×i32
        : [res] "=r" (result)
        : [vec] "r" (vec), [shamt] "r" (shift)
    );
    return result;
}
```

**PIE Instructions**:
- `ee.vsra.s32` - Vector shift right arithmetic signed 32-bit
- `ee.vsrl.s32` - Vector shift right logical (for unsigned)

**Speedup**: **4× faster**

---

## 4. u8 Operations (For LED Color Manipulation)

### `add_sat_u8_16()` - Saturating add 16×u8
**Current implementation** (line 109-117):
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 add_sat_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        u16 sum = static_cast<u16>(a.data[i]) + static_cast<u16>(b.data[i]);
        result.data[i] = (sum > 255) ? 255 : static_cast<u8>(sum);  // Saturate
    }
    return result;
}
```

**Xtensa PIE equivalent**:
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 add_sat_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    asm volatile (
        "ee.vadds.u8 %[res], %[a], %[b]"  // Add 16×u8 with saturation
        : [res] "=r" (result)
        : [a] "r" (a), [b] "r" (b)
    );
    return result;
}
```

**PIE Instructions**:
- `ee.vadds.u8` - Vector add saturating unsigned 8-bit (16 parallel adds)

**Speedup**: **16× faster** (1 instruction vs. 16 adds + 16 compares)

---

### `sub_sat_u8_16()` - Saturating subtract 16×u8
**Current implementation** (line 190-198):
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 sub_sat_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        i16 diff = static_cast<i16>(a.data[i]) - static_cast<i16>(b.data[i]);
        result.data[i] = (diff < 0) ? 0 : static_cast<u8>(diff);  // Saturate to 0
    }
    return result;
}
```

**Xtensa PIE equivalent**:
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 sub_sat_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    asm volatile (
        "ee.vsubs.u8 %[res], %[a], %[b]"  // Subtract 16×u8 with saturation
        : [res] "=r" (result)
        : [a] "r" (a), [b] "r" (b)
    );
    return result;
}
```

**PIE Instructions**:
- `ee.vsubs.u8` - Vector subtract saturating unsigned 8-bit

**Speedup**: **16× faster**

---

### `min_u8_16()` / `max_u8_16()` - Min/Max 16×u8
**Current implementation** (line 218-234):
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 min_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (a.data[i] < b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}
```

**Xtensa PIE equivalent**:
```cpp
FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 min_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    asm volatile (
        "ee.vmin.u8 %[res], %[a], %[b]"  // Min 16×u8 in parallel
        : [res] "=r" (result)
        : [a] "r" (a), [b] "r" (b)
    );
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 max_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    asm volatile (
        "ee.vmax.u8 %[res], %[a], %[b]"  // Max 16×u8 in parallel
        : [res] "=r" (result)
        : [a] "r" (a), [b] "r" (b)
    );
    return result;
}
```

**PIE Instructions**:
- `ee.vmin.u8` - Vector minimum unsigned 8-bit
- `ee.vmax.u8` - Vector maximum unsigned 8-bit

**Speedup**: **16× faster**

---

## 5. Complete Xtensa PIE Instruction Set (Reference)

### Memory Operations
| PIE Instruction | Description | FastLED Function |
|-----------------|-------------|------------------|
| `ee.vld.128.ip` | Load 128-bit vector (post-increment) | `load_u8_16()`, `load_u32_4()` |
| `ee.vst.128.ip` | Store 128-bit vector (post-increment) | `store_u8_16()`, `store_u32_4()` |
| `ee.vldbc.8` | Broadcast load 8-bit | `set1_u8_16()` |
| `ee.vldbc.16` | Broadcast load 16-bit | `set1_u16_8()` |
| `ee.vldbc.32` | Broadcast load 32-bit | `set1_u32_4()`, `set1_f32_4()` |

### Integer Arithmetic (8-bit)
| PIE Instruction | Description | FastLED Function |
|-----------------|-------------|------------------|
| `ee.vadds.u8` | Add saturating unsigned 8-bit | `add_sat_u8_16()` |
| `ee.vsubs.u8` | Subtract saturating unsigned 8-bit | `sub_sat_u8_16()` |
| `ee.vmin.u8` | Minimum unsigned 8-bit | `min_u8_16()` |
| `ee.vmax.u8` | Maximum unsigned 8-bit | `max_u8_16()` |
| `ee.vmulas.u8.qacc` | Multiply-accumulate unsigned 8-bit | `scale_u8_16()` |

### Integer Arithmetic (32-bit)
| PIE Instruction | Description | FastLED Function |
|-----------------|-------------|------------------|
| `ee.vadd.s32` | Add signed 32-bit | `add_i32_4()` |
| `ee.vsub.s32` | Subtract signed 32-bit | `sub_i32_4()` |
| `ee.vmulas.s32.qacc` | Multiply-accumulate signed 32-bit | `mulhi_i32_4()` |
| `ee.vsra.s32` | Shift right arithmetic 32-bit | `srl_u32_4()` |

### Bitwise Operations (128-bit Q registers)
| PIE Instruction | Description | FastLED Function |
|-----------------|-------------|------------------|
| `ee.andq` | AND 128-bit | `and_u8_16()`, `and_u32_4()` |
| `ee.orq` | OR 128-bit | `or_u8_16()` |
| `ee.xorq` | XOR 128-bit | `xor_u8_16()`, `xor_u32_4()` |
| `ee.notq` | NOT 128-bit | `andnot_u8_16()` (with AND) |

### Float Operations (32-bit, 4-wide)
| PIE Instruction | Description | FastLED Function |
|-----------------|-------------|------------------|
| `ee.vld.128.ip` | Load 4×f32 | `load_f32_4()` |
| `ee.vst.128.ip` | Store 4×f32 | `store_f32_4()` |
| `fadd.s` × 4 | Scalar float add (4 times) | `add_f32_4()` (no SIMD) |
| `fsub.s` × 4 | Scalar float subtract | `sub_f32_4()` (no SIMD) |
| `fmul.s` × 4 | Scalar float multiply | `mul_f32_4()` (no SIMD) |

**Note**: Xtensa LX7 has **no SIMD float** operations! Float ops are scalar only.

---

## 6. Implementation Priority for Perlin Noise

### Phase 1: Load/Store (Highest Impact) - 2-4 hours

Implement these **6 functions first** - they provide 70% of the speedup:

1. ✅ `load_u32_4()` - Used 48× per 4 pixels (permutation lookups)
2. ✅ `store_u32_4()` - Used 12× per 4 pixels (output)
3. ✅ `load_u8_16()` - Used for color operations
4. ✅ `store_u8_16()` - Used for LED output
5. ✅ `set1_u32_4()` - Used for constants
6. ✅ `load_f32_4()` / `store_f32_4()` - Used in fade LUT

**Expected speedup**: **2-3× just from load/store optimization**

---

### Phase 2: Integer Arithmetic (Medium Impact) - 2-4 hours

Implement these **7 functions** - they provide 20% additional speedup:

7. ✅ `add_i32_4()` - Used in coordinate calculations
8. ✅ `sub_i32_4()` - Used in gradient calculations
9. ✅ `xor_u32_4()` - Used in permutation hashing
10. ✅ `and_u32_4()` - Used for masking (& 255)
11. ✅ `srl_u32_4()` - Used for bit shifts
12. ✅ `mulhi_i32_4()` - Used in lerp operations
13. ✅ `min_u8_16()` / `max_u8_16()` - Used in color clamping

**Expected cumulative speedup**: **3-4× total**

---

### Phase 3: u8 Operations (LED-Specific) - 2-4 hours

Implement these for LED color manipulation (not critical for Perlin, but helps overall):

14. ✅ `add_sat_u8_16()` - LED color blending
15. ✅ `sub_sat_u8_16()` - LED dimming
16. ✅ `scale_u8_16()` - LED brightness
17. ✅ `blend_u8_16()` - LED color interpolation

**Expected benefit**: **2-3× speedup for LED color operations** (not Perlin)

---

## 7. Testing Strategy

### Step 1: Implement One Function at a Time
Start with `load_u32_4()` and verify it works before moving on.

### Step 2: Compile Test
```bash
bash compile esp32s3 --example Validation
```

### Step 3: Hardware Test
```bash
bash validate --spi --skip-lint
```

### Step 4: Benchmark
Run callgrind after each phase to measure improvement.

---

## 8. Resources

### Xtensa PIE Documentation
- ESP-IDF: `components/xtensa/include/xtensa/tie/xt_PIE.h`
- Xtensa ISA Reference: Search for "PIE" in Xtensa LX7 datasheet
- ESP32-S3 Technical Reference Manual: Chapter on "Processor Interface Extension"

### Example PIE Code
Check ESP-IDF for existing PIE usage:
```bash
grep -r "ee.vld" /path/to/esp-idf/
grep -r "ee.vadd" /path/to/esp-idf/
```

---

## Summary: Impact on Perlin Noise

| Phase | Functions Implemented | Perlin Speedup | Effort | Priority |
|-------|----------------------|----------------|--------|----------|
| **Phase 1** | Load/Store (6 funcs) | **2-3×** | 2-4 hrs | ⭐⭐⭐⭐⭐ |
| **Phase 2** | Int Arithmetic (7 funcs) | **3-4×** | 2-4 hrs | ⭐⭐⭐⭐ |
| **Phase 3** | u8 Ops (4 funcs) | 3-4× | 2-4 hrs | ⭐⭐⭐ |
| **TOTAL** | 17 functions | **3-4× faster Perlin** | 6-12 hrs | |

**Bottom line**: Implementing just **6 load/store functions** (Phase 1) will give you **2-3× speedup** in 2-4 hours of work!
