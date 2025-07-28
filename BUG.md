# UISlider Step Value JSON Serialization Bug

## Problem Description

The `UISlider` step value is incorrectly serialized as `0` instead of the actual value (e.g., `0.01f`) in JSON output, causing UI controls to malfunction.

## Bug Location

**File:** `examples/XYPath/direct.h`  
**Line:** 52  
```cpp
UISlider length("Length", 1.0f, 0.0f, 1.0f, 0.01f);
```

## Expected vs Actual Output

**Expected JSON:**
```json
{
  "name": "Length",
  "id": 4,
  "min": 0,
  "type": "slider",
  "max": 1,
  "step": 0.01,
  "group": "",
  "value": 1
}
```

**Actual JSON:**
```json
{
  "name": "Length", 
  "id": 4,
  "min": 0,
  "type": "slider",
  "max": 1,
  "step": 0,        ← BUG: Should be 0.01
  "group": "",
  "value": 1
}
```

## Root Cause Analysis

### Test Results

Created test `tests/test_ui_slider_step_bug.cpp` which reveals:

```
JSON float precision test: stored 0.01f, retrieved 0.000000
```

This confirms the bug is in the **JSON system itself**, not the UISlider code.

### Technical Analysis

1. **UISlider Constructor:** ✅ Works correctly - receives `0.01f` as step value
2. **JsonSliderImpl Constructor:** ✅ Works correctly - passes step to `JsonUiSliderInternal`
3. **JsonUiSliderInternal Constructor:** ✅ Works correctly - sets `mStep = 0.01f` and `mStepExplicitlySet = true`
4. **JsonUiSliderInternal::toJson():** ✅ Logic is correct - calls `json.set("step", mStep)` when `mStepExplicitlySet` is true
5. **Json::set(key, float):** ❌ **BUG HERE** - Float value gets corrupted during storage/retrieval

### Reproduction Steps

1. Create any `UISlider` with explicit step value: `UISlider("Test", 1.0f, 0.0f, 1.0f, 0.01f)`
2. Serialize to JSON via WASM UI system
3. Observe step value is `0` instead of `0.01`

### Test Case

```cpp
TEST_CASE("JSON float precision bug") {
    Json testJson;
    testJson.set("step_test", 0.01f);
    
    float retrieved = testJson["step_test"] | 0.0f;
    CHECK_EQ(retrieved, 0.01f);  // FAILS: retrieved is 0.000000
}
```

## Impact

- **UI Controls:** Sliders have step=0, making them non-functional
- **User Experience:** Fine-grained controls become unusable
- **Examples Affected:** All examples using UISlider with explicit step values
  - `examples/XYPath/direct.h`
  - `examples/XYPath/complex.h` 
  - `examples/Audio/advanced/advanced.h`
  - And many others with `0.01f` step values

## Investigation Focus

The bug is in the JSON system's handling of small float values. Investigation needed in:

1. **Json::set(const fl::string& key, float value)** implementation
2. **Json retrieval operators** (`operator|`)
3. **Float-to-double conversion** and precision handling
4. **JsonValue variant storage** for double values

## Files to Investigate

- `src/fl/json.h` - Json class implementation
- `src/fl/json.cpp` - Json method implementations  
- `src/platforms/shared/ui/json/slider.cpp` - JsonUiSliderInternal::toJson()

## Root Cause (SOLVED)

**Missing template specialization** in `DefaultValueVisitor` for floating point to floating point conversion.

The JsonValue variant stores floats as `double`, but when retrieving with `| 0.0f` (float fallback), there was no conversion from `double` to `float`.

## The Fix

**File:** `src/fl/json.h`  
**Added missing template specialization:**

```cpp
// Special handling for floating point to floating point conversion
template<typename U>
typename fl::enable_if<fl::is_floating_point<T>::value && fl::is_floating_point<U>::value && !fl::is_same<T, U>::value, void>::type
operator()(const U& value) {
    // Convert between floating point types (e.g., double to float)
    T converted = static_cast<T>(value);
    static T storage;
    storage = converted;
    result = &storage;
}
```

## Status

- ✅ **Bug Identified:** JSON float precision issue
- ✅ **Root Cause Found:** Missing template specialization in DefaultValueVisitor  
- ✅ **Test Created:** `tests/test_ui_slider_step_bug.cpp`
- ✅ **Fix Implemented:** Added floating point to floating point conversion
- ✅ **Fix Verified:** All tests pass, JSON now correctly handles `0.01f` → `0.01`

## Priority

**RESOLVED** - UISlider step values now serialize correctly in JSON output.
