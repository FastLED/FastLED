# Known Bugs

## JPEG Decoder Returns All Black Pixels

**Status**: RESOLVED ✅
**Discovered**: 2025-09-23
**Resolved**: 2025-09-23
**File**: `src/fl/codec/jpeg.cpp`

### Description
The JPEG decoder was failing to properly decode image data, returning frames with all black pixels (0,0,0) despite reporting successful decoding.

### Resolution
The issue has been resolved. The JPEG decoder now correctly decodes pixel data as verified by running `bash test codec`:

**Test Results** (2x2 JPEG test image):
```
Red: (248,0,0) - ✅ Correct high red, low green/blue values
White: (248,252,248) - ✅ Correct high RGB values
Blue: (0,0,248) - ✅ Correct low red/green, high blue values
Black: (0,0,0) - ✅ Correct zero values
```

### Previous Issues (Now Fixed)
- ❌ Was returning `DecodeResult::Success` but filling frame buffer with zeros
- ❌ Tests were passing due to weak verification (only checking 0-255 range)
- ❌ Early return in test_codec.cpp was masking decoder failures
- ✅ All issues resolved - decoder now properly transfers RGB565 data to CRGB format

### Root Cause (Fixed)
The `TJpgDecoder::outputCallback` function now correctly:
1. ✅ Receives RGB565 data from the TJpg_Decoder library
2. ✅ Converts RGB565 to CRGB format with proper bit scaling
3. ✅ Writes directly to Frame's RGB buffer at correct pixel positions
4. ✅ Callback invocation tracking prevents silent failures

### Verification
Run: `bash test codec` - All assertions now pass with correct pixel values.

### Related Code (Working)
- `src/fl/codec/jpeg.cpp:330-383` - outputCallback implementation (working)
- `src/fl/codec/jpeg.cpp:372-378` - RGB565 to CRGB conversion (working)
- Recent fixes in commits resolved null pointer handling and pixel data transfer

---

## GIF Decoder Had Artificial Test Passing

**Status**: RESOLVED ✅
**Discovered**: 2025-09-23
**Resolved**: 2025-09-23
**File**: `src/third_party/libnsgif/software_decoder.cpp`, `tests/test_codec.cpp`

### Description
The GIF decoder tests were artificially passing due to weak validation that only checked for null pointers and basic frame properties, without verifying actual pixel values. This masked a critical bitmap format issue.

### Issues Found and Fixed

#### 1. **Weak Test Validation** ✅ FIXED
- **Problem**: Test only checked `pixels != nullptr` and frame dimensions
- **Fix**: Added comprehensive pixel value verification like JPEG test
- **Now tests**: Red/white/blue/black pixel color values with proper assertions

#### 2. **Bitmap Format Mismatch** ✅ FIXED
- **Problem**: Used `NSGIF_BITMAP_FMT_RGBA8888` which had ambiguous byte ordering
- **Fix**: Changed to `NSGIF_BITMAP_FMT_R8G8B8A8` for explicit byte order (0xRR, 0xGG, 0xBB, 0xAA)
- **Result**: Pixel decoding now works correctly

#### 3. **Missing Error Validation** ✅ FIXED
- **Problem**: `convertBitmapToFrame()` had no null checks or bitmap validation
- **Fix**: Added comprehensive validation:
  - Null bitmap checks with error messages
  - Bitmap data validation (pixels, width, height)
  - Frame validity verification

### Test Results (2x2 GIF test image)
**Before Fix**: Red:(255,0,0), White:(255,255,255), Blue:(255,255,255), Black:(255,0,0) ❌
**After Fix**: Red:(255,0,0), White:(255,255,255), Blue:(0,0,255), Black:(0,0,0) ✅

### Root Cause Summary
1. **Weak assertions** allowed broken decoder to pass tests
2. **Wrong bitmap format** (`RGBA8888` vs `R8G8B8A8`) caused color channel corruption
3. **No bitmap validation** masked underlying libnsgif integration issues

### Additional Fix: RGB565 to RGB888 Scaling Issue ✅ FIXED

**Problem**: JPEG decoder showed incorrect white pixels (248,252,248) instead of (255,255,255) due to improper RGB565 scaling
**Root Cause**: RGB565 → RGB888 conversion used simple bit-shifting instead of proper scaling
- Old: `(value >> 11) << 3` gave 0-248 instead of 0-255 for 5-bit values
- Old: `((value >> 5) & 0x3F) << 2` gave 0-252 instead of 0-255 for 6-bit values

**Fix**: Implemented proper scaling with lookup tables for performance
- 5-bit values: Scale 0-31 → 0-255 using `(value * 255) / 31`
- 6-bit values: Scale 0-63 → 0-255 using `(value * 255) / 63`
- Used static lookup tables to avoid divisions per pixel

**Files Fixed**:
- `src/fx/frame.h:42-67` - Added `rgb565ToRgb888()` with lookup tables
- `src/fl/codec/jpeg.cpp:371-374` - Updated JPEG decoder to use lookup function
- `src/fl/codec/jpeg.cpp:405-407` - Updated `convertPixelFormat()` function

### Final Verification
Run: `bash test codec` - Both JPEG and GIF tests now pass with perfect pixel values:
- **JPEG**: Red:(255,0,0), White:(255,255,255), Blue:(0,0,255), Black:(0,0,0) ✅
- **GIF**: Red:(255,0,0), White:(255,255,255), Blue:(0,0,255), Black:(0,0,0) ✅

### Related Code (Fixed)
- `src/third_party/libnsgif/software_decoder.cpp:206` - Fixed bitmap format specification
- `src/third_party/libnsgif/software_decoder.cpp:279-317` - Enhanced convertBitmapToFrame validation
- `tests/test_codec.cpp:239-308` - Comprehensive GIF pixel value testing


### Additinal

  * tests/test_codec.cpp should comprehensively test all the RGB 565 convers values.
    * we only have to do one component at a time
  * tests/test_codec.cpp should test the animated gif using the IDecoder interface. Test that there are two frames.
  * 
