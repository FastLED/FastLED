# UISlider Virtual Function Linking Issue - Bug Report

## Summary

The XYPath example fails to link with error: `undefined symbol: public: virtual void __cdecl fl::UISlider::Listener::onBeginFrame(void)` when using UISlider components on the STUB platform.

## Environment

- **Platform**: Windows 10 with git-bash
- **Compiler**: Clang 19.1.0
- **Build System**: FastLED Simple Build System with unified compilation (`FASTLED_ALL_SRC=1`)
- **Target Platform**: STUB (testing platform)
- **Example**: XYPath (`examples/XYPath/XYPath.ino`)

## Error Details

```
[LINKING] FAILED: XYPath.exe: lld-link: error: undefined symbol: public: virtual void __cdecl fl::UISlider::Listener::onBeginFrame(void)
>>> referenced by .build\examples\XYPath\XYPath.o:(const fl::UISlider::Listener::`vftable')
```

## 🎯 FINAL ROOT CAUSE: Object File Name Collisions

**The issue was caused by multiple `ui.cpp` files overwriting each other during library compilation due to identical object file names.**

### Multiple ui.cpp Files Found

The build system discovered three `ui.cpp` files:
1. **`src/fl/ui.cpp`** - Main UI implementation containing `UISlider::Listener::onBeginFrame()`
2. **`src/platforms/shared/ui/json/ui.cpp`** - JSON UI implementation  
3. **`src/platforms/wasm/ui.cpp`** - WASM UI implementation

### Object File Collision Problem

**Original faulty logic** in `ci/test_example_compilation.py:813`:
```python
obj_file = obj_dir / f"{cpp_file.stem}.o"  # Only uses filename stem!
```

**Result**: All three files generated `ui.o`, with each subsequent compilation overwriting the previous one:
- `src/fl/ui.cpp` → `ui.o` ✅ (contains UISlider symbols)  
- `src/platforms/shared/ui/json/ui.cpp` → `ui.o` ❌ (overwrites previous)
- `src/platforms/wasm/ui.cpp` → `ui.o` ❌ (final overwrite, no UISlider symbols)

### Evidence of the Problem

**Before Fix:**
- ❌ No `ui.o` found in `.build/fastled/obj/` directory (only `ui_internal.o` and `ui_manager.o`)
- ❌ Archive claimed to contain `ui.o` but it was the wrong one (last file to overwrite)
- ❌ Library contained 0 UISlider symbols
- ❌ Missing symbol: `?onBeginFrame@Listener@UISlider@fl@@UEAAXXZ`

**Manual compilation verification:**
```bash
clang++ -std=c++14 -DSKETCH_HAS_LOTS_OF_MEMORY=1 -DFASTLED_HAS_ENGINE_EVENTS=1 -c src/fl/ui.cpp
nm test_ui.o | grep onBeginFrame
# Result: 0000000000000070 T ?onBeginFrame@Listener@UISlider@fl@@UEAAXXZ ✅
```

This proved the source code and defines were correct, but the build system had a file collision issue.

## ✅ SOLUTION: Unique Object File Naming

**Fixed the object file naming logic** in `ci/test_example_compilation.py` to include full relative paths:

```python
# Create unique object file name by including relative path to prevent collisions
# Convert path separators to underscores to create valid filename
src_dir = Path("src")
if cpp_file.is_relative_to(src_dir):
    rel_path = cpp_file.relative_to(src_dir)
else:
    rel_path = cpp_file

# Replace path separators with underscores for unique object file names
obj_name = str(rel_path.with_suffix('.o')).replace('/', '_').replace('\\', '_')
obj_file = obj_dir / obj_name
```

**Result**: Each `ui.cpp` file now generates a unique object file:
- `src/fl/ui.cpp` → **`fl_ui.o`** ✅ (contains UISlider symbols)
- `src/platforms/shared/ui/json/ui.cpp` → **`platforms_shared_ui_json_ui.o`** ✅ 
- `src/platforms/wasm/ui.cpp` → **`platforms_wasm_ui.o`** ✅

## ✅ VERIFICATION: Fix Success

**After Fix:**
- ✅ **[LIBRARY] Successfully compiled 129/129 FastLED sources**
- ✅ **[LINKING] SUCCESS: XYPath.exe**
- ✅ **`fl_ui.o` contains 38 UISlider symbols and 5 onBeginFrame symbols**
- ✅ **Specific symbol present**: `0000000000000070 T ?onBeginFrame@Listener@UISlider@fl@@UEAAXXZ`

**Final verification:**
```bash
bash test --examples xypath --full
# Result: SUCCESS - No linking errors
```

## Files Modified

- **`ci/test_example_compilation.py`** - Fixed object file naming logic to prevent collisions

## Related Issues

This fix resolves potential object file collisions for **any source files with identical names** in different directories, not just UI components. The build system now properly handles:

- Multiple `ui.cpp` files
- Multiple files with common names across different platform directories
- Any future source file naming conflicts

## Architecture Lessons

1. **Build systems must ensure unique object file names** when compiling from multiple directories
2. **File stem alone is insufficient** for object file naming in complex directory structures  
3. **Object file collisions can silently overwrite symbols** leading to mysterious linking errors
4. **Always test linking with full program creation**, not just compilation success

## Status

- **Investigation**: ✅ Complete
- **Root Cause**: ✅ **IDENTIFIED** - Object file name collisions from multiple `ui.cpp` files
- **Fix**: ✅ **IMPLEMENTED** - Unique object file naming with full relative paths
- **Verification**: ✅ **CONFIRMED** - XYPath example compiles and links successfully
- **Issue**: ✅ **RESOLVED**
