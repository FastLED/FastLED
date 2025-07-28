# XYPath Slider Step Serialization Bug Analysis

## Problem Description
The UI sliders in `examples/XYPath/direct.h` are not correctly serializing their step values when displayed in the browser. Small floating point step values (like 0.01) are being truncated to 0, while larger values (like 1.0) display correctly.

## What the Code Says
In `examples/XYPath/direct.h`, the sliders are defined as:
```cpp
UISlider offset("Offset", 0.0f, 0.0f, 1.0f, 0.01f);
UISlider steps("Steps", 100.0f, 1.0f, 200.0f, 1.0f);  
UISlider length("Length", 1.0f, 0.0f, 1.0f, 0.01f);
```

## Expected JSON Output
```json
{
  "name": "Offset",
  "step": 0.01,  // ← Should be 0.01
  // ... other fields
}
```

## Actual Browser JSON Output
```json
{
  "name": "Offset", 
  "step": 0,     // ← Incorrectly truncated to 0
  // ... other fields
}
```

## Analysis Results

### ✅ C++ JSON Generation is CORRECT
Created unit test `XYPath slider step serialization bug` in `tests/test_ui.cpp` which shows:
- C++ correctly generates: `"step":0.010000"` for 0.01f values
- C++ correctly generates: `"step":1.000000"` for 1.0f values  
- The issue is **NOT** in the C++ slider serialization code

### ❌ Browser/JavaScript JSON Processing has BUG  
The JSON truncation happens between:
1. **C++ generates:** `{"step":0.010000}` ✅ Correct
2. **Browser displays:** `{"step":0}` ❌ Truncated

## Root Cause
The bug appears to be in the **JavaScript/browser JSON processing pipeline** where small floating point values are being converted to integers, likely during:
- JSON parsing/serialization on the JavaScript side
- UI framework number formatting  
- JSON pretty-printing or display logic

## Bug Impact
- **Offset slider**: step=0.01 becomes step=0 (makes slider unusable for fine adjustments)
- **Length slider**: step=0.01 becomes step=0 (makes slider unusable for fine adjustments)  
- **Steps slider**: step=1.0 remains step=1 (works correctly)

## Next Steps
The issue is in the browser/JavaScript JSON processing, not the C++ code. Need to investigate:
1. JavaScript JSON parsing logic
2. UI slider display/formatting code
3. Any JSON serialization that happens on the frontend

---

## Original Browser Output for Reference
[
  {
    "name": "Simple control of an xy path",
    "id": 0,
    "type": "title",
    "text": "Simple control of an xy path",
    "group": ""
  },
  {
    "name": "description",
    "id": 1,
    "type": "description",
    "text": "This is more of a test for new features.",
    "group": ""
  },
  {
    "name": "Offset",
    "id": 2,
    "min": 0,
    "type": "slider",
    "max": 1,
    "step": 0,        ← BUG: Should be 0.01
    "group": "",
    "value": 0
  },
  {
    "name": "Steps",
    "id": 3,
    "min": 1,
    "type": "slider",
    "max": 200,
    "step": 1,        ← CORRECT: 1.0 → 1
    "group": "",
    "value": 100
  },
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
]
