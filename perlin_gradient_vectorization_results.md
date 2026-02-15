# Perlin Gradient Vectorization Results

## Summary: Gradient Vectorization is SLOWER ❌

**Tested**: Vectorizing gradient computation (`grad()` function)
**Result**: **7% SLOWER** than scalar implementation

| Implementation | Median ns/call | vs Scalar | Status |
|----------------|---------------|-----------|--------|
| **Scalar gradients** | 50,480 ns | 1.00× | ✅ **Fastest** |
| **Vectorized gradients** | 54,150 ns | 0.93× | ❌ **7% slower** |

---

## What Was Tested

### The `grad()` Function

```cpp
static FASTLED_FORCE_INLINE fl::i32 grad(int hash, fl::i32 x, fl::i32 y) {
    struct GradCoeff { fl::i8 cx; fl::i8 cy; };
    constexpr GradCoeff lut[16] = {
        { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1},
        { 1,  0}, {-1,  0}, { 1,  0}, {-1,  0},
        { 0,  1}, { 0, -1}, { 0,  1}, { 0, -1},
        { 1,  1}, { 0, -1}, {-1,  1}, { 0, -1},
    };
    const GradCoeff &g = lut[hash & 15];  // LUT lookup
    return g.cx * x + g.cy * y;            // 2 multiplies + 1 add
}
```

**Current usage**: 16 gradient calls per 4 pixels
- 4 pixels × 4 corners (aa, ba, ab, bb) = 16 total

---

## Vectorization Approach

### Idea
User correctly observed that gradient computation has two parts:
1. **Permutation lookup** (scalar): `hash = perm[AA[i] & 255]`
2. **Gradient arithmetic** (vectorizable?): `cx * x + cy * y`

### Implementation
Attempted to vectorize the arithmetic while keeping permutation lookups scalar:

```cpp
// For each corner (aa, ba, ab, bb):
for (int i = 0; i < 4; i++) {
    const GradCoeff &g = grad_lut[perm[AA[i] & 255] & 15];
    cx[i] = g.cx;  // Scalar: Store coefficient
    cy[i] = g.cy;
}

// SIMD: Vectorize arithmetic
simd_u32x4 cx_vec = load_u32_4(cx);
simd_u32x4 cy_vec = load_u32_4(cy);
simd_u32x4 result = cx_vec * x_vec + cy_vec * y_vec;  // 4× parallel
```

---

## Why It's Slower

### Memory Traffic Analysis

**Scalar version** (16 gradient calls):
- 16 LUT lookups (coefficient table)
- 16 permutation lookups (already counted in perm stage)
- 32 multiplies (inline, register-to-register)
- 16 adds (inline, register-to-register)
- **Total memory**: 16 LUT loads

**Vectorized version** (4 SIMD blocks):
- 16 permutation lookups (same)
- 16 LUT lookups (same)
- **16 scalar stores** (cx[i], cy[i] arrays) ← **NEW OVERHEAD!**
- 8 SIMD loads (cx_vec, cy_vec × 4 corners) ← **NEW OVERHEAD!**
- 8 SIMD multiplies (vectorized)
- 4 SIMD adds (vectorized)
- 4 SIMD stores (results)
- **Total memory**: 16 LUT loads + 16 stores + 8 SIMD loads + 4 SIMD stores

### The Problem

**Setup overhead dominates small computation:**
1. **16 scalar stores** (cx[i]/cy[i]) to build coefficient arrays
2. **8 SIMD loads** to read coefficient arrays back into SIMD registers
3. This memory traffic outweighs the benefit of vectorized arithmetic

**Computation is too simple:**
- Each gradient: 2 multiplies + 1 add = **3 operations**
- Setup cost: 1 store + 1 load per coefficient = **4 memory ops** (more than computation!)

---

## Instruction-Level Breakdown

### Scalar: Direct Computation
```cpp
g_aa[i] = grad(perm[AA[i]], x_frac[i], y_frac[i]);
// 1. perm lookup (already done)
// 2. LUT lookup: lut[hash & 15] → registers (cx, cy)
// 3. Multiply: cx * x → register
// 4. Multiply: cy * y → register
// 5. Add: result = mul1 + mul2 → register
// 6. Store: g_aa[i] = result

// All arithmetic is register-to-register (fast!)
```

### Vectorized: Array Indirection
```cpp
// 1-4: perm + LUT lookups (same)
cx[i] = g.cx;  // 5. Store to memory (SLOW!)
cy[i] = g.cy;  // 6. Store to memory (SLOW!)

// Later:
cx_vec = load(cx);  // 7. Load from memory (SLOW!)
cy_vec = load(cy);  // 8. Load from memory (SLOW!)
result = cx_vec * x_vec + cy_vec * y_vec;  // 9-11. Vectorized arithmetic

// Extra memory round-trip: register → memory → SIMD register
```

---

## Performance Impact

### Time Breakdown (Estimated)

**Scalar gradient** (per call):
- LUT lookup: 3 cycles (L1 cache)
- 2 multiplies: 6 cycles (pipelined)
- 1 add: 1 cycle
- **Total**: ~10 cycles/gradient × 16 = **160 cycles**

**Vectorized gradient** (per block of 4):
- 4 LUT lookups: 12 cycles
- 4 scalar stores: 16 cycles (L1 write)
- 2 SIMD loads: 8 cycles (read cx/cy arrays)
- 2 SIMD multiplies: 6 cycles (vectorized)
- 1 SIMD add: 1 cycle
- **Total**: ~43 cycles/block × 4 blocks = **172 cycles**

**Overhead**: +12 cycles (7% slower) ✅ Matches observed result!

---

## Key Lessons

### When SIMD Helps
✅ **Large, compute-intensive operations**
- Example: 3-level lerp (9 ops) → 4× parallel speedup
- Example: Coordinate extraction (multiple shifts/masks) → 4× parallel

✅ **Data already in SIMD-friendly layout**
- Example: x_frac[4], y_frac[4] arrays already exist → direct SIMD load

### When SIMD Hurts
❌ **Small computations with setup overhead**
- Example: 2 multiplies + 1 add (too simple!)
- Setup cost (stores + loads) > computation benefit

❌ **Non-SIMD data layout requiring conversion**
- Example: cx/cy pairs scattered in LUT → need scalar stores to build arrays
- Memory round-trip overhead dominates

---

## Comparison to Other Failed Optimizations

| Optimization | Result | Reason |
|--------------|--------|--------|
| **AVX2 gather** (perm) | 3-5% slower | Gather latency > scalar loads |
| **Vectorized gradients** | 7% slower | Setup overhead > arithmetic benefit |
| ✅ **SSE2 lerp/coord** | 46% faster | Large computation, data ready for SIMD |

**Pattern**: SIMD helps for **large, vectorizable computations** but hurts for **small ops with setup overhead**

---

## Final Decision

**Keep scalar gradient computation** - it's faster!

The gradient function is too simple (3 ops) and requires too much setup (array building) to benefit from SIMD vectorization.

---

## Related Documentation

- `perlin_avx2_gather_results.md` - Why AVX2 gather is slower
- `perlin_simd_optimization_summary.md` - Overall SIMD optimization results

---

## Conclusion

Not all vectorizable code should be vectorized! The gradient computation is a perfect example of where **scalar code is faster** due to:
1. Small computation size (2 multiplies + 1 add)
2. High setup overhead (building coefficient arrays)
3. Memory traffic dominates computation

**Rule of thumb**: Only vectorize if computation >> setup cost (at least 5-10 ops per SIMD operation)
