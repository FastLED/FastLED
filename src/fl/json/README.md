# FastLED JSON Parser

High-performance, memory-efficient JSON parser for embedded systems with zero-dependency, two-phase parsing architecture.

## Overview

FastLED's custom JSON parser (`parse2()`) is designed for memory-constrained environments, offering significant improvements over traditional parsers:

**Performance**:
- **1.62x faster** parsing (62% speedup)
- **62.5% higher throughput** (51.58 MB/s vs 31.75 MB/s)

**Memory Efficiency**:
- **45% fewer allocations** than ArduinoJson
- **24% lower peak memory** usage (56.9 KB vs 74.7 KB)
- **Zero heap allocations** during validation phase

**Architecture**:
- **Two-phase parsing** (validate → build)
- **100% API compatible** with existing `Json` interface
- **Optimized for embedded systems** (608 bytes stack overhead)

## Performance Comparison

### Memory Profiling (2.3KB JSON, 300+ LED coordinates)

| Metric | ArduinoJson `parse()` | Custom `parse2()` | Improvement |
|--------|----------------------|-------------------|-------------|
| **Peak Memory** | 74,705 bytes | 56,951 bytes | **-17,754 bytes (-23.8%)** |
| **Total Allocated** | 90,967 bytes | 58,751 bytes | **-32,216 bytes (-35.4%)** |
| **Allocations** | 243 | 133 | **-110 (-45.3%)** |
| **Phase 1 (Validation)** | N/A | **0 allocations** | ✅ Zero-copy |
| **Memory Leaks** | 0 | 0 | ✅ None |

### Test Data Characteristics

- **Size**: 2,344 bytes JSON
- **Structure**: 3 LED strips × 100 coordinates each
- **Complexity**: Deeply nested objects, mixed arrays (int16, floats, strings)
- **Node Count**: ~120-140 `JsonValue` objects

<details>
<summary><b>Click to view test JSON (2.3KB ScreenMap)</b></summary>

```json
{
  "version": "1.0",
  "fps": 60,
  "brightness": 0.85,
  "strips": [
    {
      "id": "strip_0",
      "type": "WS2812B",
      "length": 100,
      "x": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99],
      "y": [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
      "diameter": 0.5,
      "color_order": "RGB"
    },
    {
      "id": "strip_1",
      "type": "APA102",
      "length": 100,
      "x": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99],
      "y": [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
      "diameter": 0.3,
      "color_order": "BGR"
    },
    {
      "id": "strip_2",
      "type": "WS2812B",
      "length": 100,
      "x": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99],
      "y": [2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2],
      "diameter": 0.5,
      "color_order": "RGB"
    }
  ],
  "effects": [
    {"name": "rainbow", "speed": 1.5, "brightness": 0.9},
    {"name": "twinkle", "speed": 2.0, "brightness": 0.7},
    {"name": "fade", "speed": 0.5, "brightness": 1.0}
  ],
  "metadata": {
    "created": "2024-01-15",
    "author": "FastLED",
    "description": "Memory profiling stress test JSON"
  }
}
```

</details>

## Architecture

### Two-Phase Parsing

**Phase 1: Validation (Zero-Copy)**
- Tokenizes input with `fl::span<const char>` views (no string allocations)
- Validates JSON structure (balanced brackets, correct syntax)
- Stack tracking with `fl::vector_inlined<char, 32>` (inline up to depth 32)
- **0 heap allocations** during this phase

**Phase 2: Building**
- Re-tokenizes with same tokenizer
- Builds `JsonValue` tree using `fl::shared_ptr<JsonValue>`
- Type detection for specialized arrays (`vector<i16>`, `vector<u8>`, `vector<float>`)
- **Single allocation per node** via `make_shared` control block optimization

### Key Optimizations

#### 1. make_shared Control Block Inlining
- **Standard `shared_ptr`**: 2 allocations (control block + object)
- **Optimized `make_shared`**: 1 allocation (inlined control block + object)
- **Impact**: 45% reduction in allocations (~110 saved for typical JSON document)

#### 2. String Interning
- Deduplicates repeated strings (e.g., "RGB", "WS2812B") via hash map
- `fl::unordered_map` with inline storage (8 entries)
- O(1) average lookup time

#### 3. Specialized Array Detection
- Homogeneous integer arrays → `vector<i16>` or `vector<u8>`
- Homogeneous float arrays → `vector<float>`
- Reduces memory overhead for LED coordinate arrays

#### 4. Zero-Copy Tokenization
- `fl::span<const char>` views into original input string
- No temporary string allocations during Phase 1
- Lazy unescaping (only when building Phase 2)

## Usage

```cpp
#include "fl/json/json.h"

// Parse JSON (automatically uses optimized parse2())
const char* json_str = R"({"brightness": 0.85, "fps": 60})";
Json doc = Json::parse(json_str);

// Access values
float brightness = doc["brightness"].as<float>().value_or(1.0f);
int fps = doc["fps"].as<int>().value_or(30);

// Validation only (zero heap allocations)
bool valid = Json::parse2_validate_only(fl::string_view(json_str));
```

## Memory Budget

| Component | Stack Overhead | Notes |
|-----------|---------------|-------|
| Tokenizer | 32 bytes | State machine + span |
| Validator | 64 bytes | Inline bracket stack (depth 32) |
| Builder | 512 bytes | 16 stack frames × 32 bytes |
| **Total** | **~608 bytes** | During parsing only |

## Implementation Details

### Tokenizer State Machine

**States**: `START`, `IN_STRING`, `STRING_ESCAPE`, `IN_NUMBER`, `IN_TRUE_*`, `IN_FALSE_*`, `IN_NULL_*`, `DONE`, `ERROR_STATE`

**Tokens**: `LBRACE`, `RBRACE`, `LBRACKET`, `RBRACKET`, `COLON`, `COMMA`, `STRING`, `NUMBER`, `TRUE`, `FALSE`, `NULL_VALUE`, `END_OF_INPUT`, `ERROR`

### Visitor Pattern

```cpp
class JsonVisitor {
public:
    virtual ParseState on_token(JsonToken token, const fl::span<const char>& value) = 0;
    virtual ~JsonVisitor() = default;
};

// Phase 1: Validator (minimal state)
class JsonValidator : public JsonVisitor { /* ... */ };

// Phase 2: Builder (complex state, tree construction)
class JsonBuilder : public JsonVisitor { /* ... */ };
```

### Recursion Protection

- **Max nesting depth**: 32 levels
- **Fatal error on exceed**: Prevents stack overflow
- **Detection**: Both validator and builder track depth counter

## Testing

Comprehensive test coverage with A/B validation:

```bash
# Memory profiling (allocation tracking)
bash test json_memory_profile --cpp

# Standard tests (ArduinoJson vs parse2() comparison)
bash test json --cpp

# make_shared optimization tests
bash test shared_ptr_make_shared --cpp
```

## Benchmark Results

### Memory Efficiency

For a typical FastLED ScreenMap JSON (2.3KB):
- **23.8% less peak memory** than ArduinoJson
- **45.3% fewer allocations** than ArduinoJson
- **Zero-copy validation** (0 allocations during Phase 1)

### Parsing Speed (2.3KB JSON, 1000 iterations)

| Metric | ArduinoJson `parse()` | Custom `parse2()` | Result |
|--------|----------------------|-------------------|--------|
| **Parse Time** | 73.61 µs | 45.31 µs | **1.62x faster** ✅ |
| **Throughput** | 31.75 MB/s | 51.58 MB/s | **+62.5%** ✅ |
| **Time Saved** | - | 28.30 µs/parse | **-38.4%** ✅ |

**Summary**: Custom `parse2()` is **62% faster** than ArduinoJson with **62.5% higher throughput**

## Future Optimizations

- [ ] SIMD-accelerated string scanning (SSE2/NEON)
- [ ] Memory pool allocator for JsonValue nodes
- [ ] Direct UTF-8 validation (skip unescaping)
- [ ] Incremental parsing for streaming JSON

## File Structure

```
fl/
├── json.h                    # Main public API (769 lines)
├── json.cpp.hpp              # Implementation
└── json/
    ├── README.md             # This file
    └── detail/
        └── types.h           # Internal types and helpers (1617 lines)
```

## References

- **Public API**: `src/fl/json.h` - Main include, clean 769-line interface
- **Implementation Details**: `src/fl/json/detail/types.h` - Internal types (JsonValue, visitors, helpers)
- **Parser Implementation**: `src/fl/json.cpp.hpp` (lines 330+)
- **Tests**: `tests/fl/json.cpp`, `tests/fl/json_memory_profile.cpp`, `tests/fl/json_performance.cpp`
- **make_shared optimization**: `src/fl/stl/shared_ptr.h`
