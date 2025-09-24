# Known Bugs

## JPEG Decoder Returns All Black Pixels

**Status**: Active
**Discovered**: 2025-09-23
**File**: `src/fl/codec/jpeg.cpp`

### Description
The JPEG decoder is failing to properly decode image data, returning frames with all black pixels (0,0,0) despite reporting successful decoding. The decoder:
- Returns `DecodeResult::Success`
- Creates a valid frame with correct dimensions (2x2 in test case)
- But fills the frame buffer with all zeros instead of actual decoded pixel data

### Test Evidence
When decoding a 2x2 JPEG test image that should contain red/white/blue/black pixels:
```
Expected: Red(>150,<100,<100), White(>200,>200,>200), Blue(<100,<100,>150), Black(<50,<50,<50)
Actual: All pixels are (0,0,0)
```

### Root Cause Investigation
The issue appears to be in the `TJpgDecoder::outputCallback` function which:
1. Receives RGB565 data from the TJpg_Decoder library
2. Converts it to the target format (RGB888) via `convertPixelFormat`
3. But the data appears to be zeros or the callback isn't being called properly

### Impact
- All JPEG decoding operations fail silently
- Tests were previously passing due to weak verification (only checking 0-255 range)
- The issue was masked by an early return in test_codec.cpp that skipped assertions on decoder failure

### Related Code
- `src/fl/codec/jpeg.cpp:312-349` - outputCallback implementation
- `src/fl/codec/jpeg.cpp:355-377` - convertPixelFormat implementation
- Recent commit 7d7390bf2b attempted to fix null pointer issues but the underlying decode problem remains

### Reproduction
Run: `bash test codec`

The test will fail with multiple assertions showing all pixels are black when they should have color values.