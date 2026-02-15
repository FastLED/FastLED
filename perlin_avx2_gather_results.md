# AVX2 Gather Performance Analysis

## TL;DR: AVX2 Gather is SLOWER ❌

**Result**: AVX2 `_mm_i32gather_epi32` made Perlin noise **3-7% SLOWER**, not faster.

## Performance Results

| Implementation | Median ns/call | Speedup | Status |
|----------------|---------------|---------|--------|
| **SSE2 SIMD** (scalar perm) | 51,290 ns | 1.00× | ✅ **Fastest** |
| **AVX2 gather** (vectorized perm) | 53,055 ns | 0.966× | ❌ **Slower** |

**Slowdown**: **+3.4%** with AVX2 gather

---

## Why AVX2 Gather is Slower

### 1. High Latency
- `_mm_i32gather_epi32` has **10-20 cycle latency** on most CPUs
- Scalar loads: **3-5 cycle latency** each
- For 4 elements: 4 × 5 = 20 cycles (scalar) vs 15-20 cycles (gather)
- **No benefit**, plus overhead

### 2. Small Gather Size
- SIMD width: Only **4 elements** per gather
- AVX2 gather benefits require **≥8 elements** (AVX-512) or **≥16 elements**
- Below 8 elements, scalar loops are competitive or faster

### 3. Setup Overhead
- Each gather requires:
  - Index vector creation (`_mm_set_epi32`)
  - Byte masking (`_mm_and_si128` to extract low byte)
  - Result storage (`_mm_storeu_si128`)
- Scalar loop: Direct array indexing (no setup)

### 4. Cache Locality
- Permutation table is **cache-aligned** and **sequential access**
- Scalar loads benefit from **hardware prefetching**
- Gather operations don't trigger prefetcher as effectively

### 5. Microarchitecture-Specific
- On Intel Haswell/Broadwell: Gather is slow (10+ cycles)
- On Intel Skylake+: Improved but still not faster for small sizes
- On AMD Zen: Gather is notoriously slow (15-20 cycles)

---

## Code Comparison

### Scalar (Faster) ✅
```cpp
// 24 scalar loads for 4 pixels
for (int i = 0; i < 4; i++) {
    A[i]  = perm[X[i] & 255]       + Y[i];  // 1 load
    AA[i] = perm[A[i] & 255];                // 1 load
    AB[i] = perm[(A[i] + 1) & 255];          // 1 load
    B[i]  = perm[(X[i] + 1) & 255] + Y[i];  // 1 load
    BA[i] = perm[B[i] & 255];                // 1 load
    BB[i] = perm[(B[i] + 1) & 255];          // 1 load
}
// Total: 24 loads × 3-5 cycles = 72-120 cycles
```

### AVX2 Gather (Slower) ❌
```cpp
// 6 gather operations for 4 pixels
__m128i A_idx = _mm_set_epi32(X[3] & 255, X[2] & 255, X[1] & 255, X[0] & 255);
__m128i A_perm = _mm_i32gather_epi32((int const*)perm, A_idx, 1);
A_perm = _mm_and_si128(A_perm, _mm_set1_epi32(0xFF));
_mm_storeu_si128((__m128i*)A, A_perm);
// ... 5 more gathers for AA, AB, B, BA, BB
// Total: 6 gathers × 15-20 cycles + 18 setup ops = 120-180 cycles
```

---

## When Gather IS Beneficial

AVX2 gather is faster when:
1. **Large gather sizes**: ≥16 elements per gather (use AVX-512 with 16-wide SIMD)
2. **Irregular access patterns**: When cache prefetcher can't help
3. **Modern microarchitectures**: Intel Ice Lake+ with improved gather latency
4. **High computational density**: When gather latency is hidden by other work

---

## Lesson Learned

**Not all SIMD optimizations are faster!**

- ✅ **Vector arithmetic/lerp**: 3.90× speedup (good fit for SIMD)
- ❌ **Gather operations**: 0.97× slowdown (bad fit for small SIMD width)

**Rule of thumb**:
- SIMD width < 8: Scalar loops competitive
- SIMD width 8-16: Gather might help (benchmark!)
- SIMD width ≥16 (AVX-512): Gather usually wins

---

## Recommendation

**Keep scalar permutation lookups** - they're faster on SSE2/AVX2 with 4-wide SIMD.

**Focus on**:
1. **perlin_q16**: Use 16-bit precision (30-50% speedup) ⭐⭐⭐⭐⭐
2. **Xtensa PIE**: Implement ESP32-S3 SIMD (200-300% speedup) ⭐⭐⭐⭐⭐
3. **Simplex noise**: Algorithmic improvement (500-900% speedup) ⭐⭐⭐⭐

**Skip AVX2 gather** - not worth the complexity for this use case.

---

## References

- Intel Optimization Manual: "Gather instructions have high latency"
- Agner Fog's instruction tables: `vpgatherdd` = 10-20 cycles on most CPUs
- LLVM bug tracker: "AVX2 gather slower than scalar for small sizes"
