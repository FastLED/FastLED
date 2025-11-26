# Agent Loop: Upgrade Emscripten Build Chain with PCH and Incremental Builds

## Current Status: Phase 1 Complete ✅

**Last Updated**: Iteration 1
**Next Task**: Phase 2 - Static Library Build

---

## Phase 1: Precompiled Header (PCH) - ✅ COMPLETE

### Status: IMPLEMENTED AND TESTED
**Completed in**: Iteration 1
**Files Created**:
- `src/platforms/wasm/compiler/wasm_pch.h` (17MB PCH header)
- `ci/wasm_compile_pch.py` (PCH compilation script with smart invalidation)
- `build/wasm/wasm_pch.h.pch` (compiled PCH)
- `build/wasm/wasm_pch.h.d` (dependency file)
- `build/wasm/wasm_pch_metadata.json` (rebuild metadata)

### ✅ Accomplishments:
1. **PCH Header Creation**: Created `wasm_pch.h` with FastLED.h, emscripten headers, and common utilities
2. **Smart Invalidation**: Implemented content hash, flags hash, and compiler version tracking
3. **Dependency Tracking**: Generated proper dependency files using `compile_pch.py` wrapper
4. **Validation**: Comprehensive PCH validation flags to catch corruption/staleness
5. **Testing**: Verified initial compilation, rebuild detection, and skip-when-up-to-date behavior

### ✅ Test Results:
- Initial PCH compilation: **~5 seconds** (17MB output)
- Incremental (up-to-date): **~0.1 seconds** (skipped)
- Content change detection: ✅ Working
- Flag change detection: ✅ Working (via metadata hash)
- Compiler version detection: ✅ Working

### ⚠️ Known Minor Issue:
- Warning: `em++: warning: linker flag ignored during compilation: '--profiling-funcs'`
- Cause: Linker flag mixed with compiler flags in TOML
- Impact: Cosmetic only (doesn't affect functionality)
- Fix: Can be addressed in future iteration if desired

### 📝 Next Agent Notes:
- **PCH is working** - don't modify unless absolutely necessary
- **Usage**: `uv run python ci/wasm_compile_pch.py --mode <mode> [--verbose] [--force]`
- **Clean**: `uv run python ci/wasm_compile_pch.py --clean`
- PCH automatically rebuilds when needed (header content, flags, or compiler changes)

---

## Phase 2: Static Library Build (FastLED Archive) - 🔜 NEXT

### Objective
Build FastLED sources once into a static library (`libfastled.a`), link with sketches.

### Key Tasks:
1. **Create Library Build Script** (`ci/wasm_build_library.py`):
   - Compile all 251 FastLED source files to object files (`.o`)
   - Use PCH from Phase 1: `-include-pch build/wasm/wasm_pch.h.pch`
   - Generate dependency files (`.d`) for each source
   - Use flags from `build_flags.toml` [all] + [library] sections
   - Create thin archive: `emar rcsT libfastled.a *.o`
   - Output: `build/wasm/libfastled.a`

2. **Implement Incremental Compilation**:
   - Check dependency files to detect which sources need rebuild
   - Only recompile sources if:
     - Source file modified
     - Header dependencies modified (from `.d` file)
     - Compiler flags changed
     - PCH invalidated
   - Parallel compilation using `concurrent.futures` or `multiprocessing`

3. **Dependency File Parsing**:
   - Parse `.d` files (Makefile format) to track header changes
   - Store in `build/wasm/deps/` directory
   - Use for smart rebuild decisions

4. **Archive Creation**:
   - Use `emar` (emscripten's ar wrapper)
   - Create **thin archive**: `rcsT` flags (T = thin)
   - Thin archives store references to objects, not copies (faster linking)

### Reference Files to Study:
- `meson.build` lines 239-245 (static library creation)
- `examples/meson.build` lines 195-204 (linking with static library)
- `ci/wasm_compile_native.py` (current source discovery via `rglob.py`)

### Expected Deliverables:
- `ci/wasm_build_library.py` - Library compilation script
- `build/wasm/libfastled.a` - Static library (thin archive)
- `build/wasm/objects/*.o` - Object files for each source
- `build/wasm/deps/*.d` - Dependency files for each source

### Testing Strategy:
1. Clean build: Delete `build/wasm/`, run library build, verify all 251 objects created
2. Incremental build: Modify one source file, verify only that object rebuilt
3. Header change: Modify common header, verify dependent objects rebuilt
4. PCH change: Modify PCH, verify all objects rebuilt

---

## Phase 3: Sketch Compilation and Linking - ⏳ PENDING

### Objective
Always rebuild sketch (small), link with cached library.

### Key Tasks:
1. **Update `ci/wasm_compile_native.py`**:
   - Compile sketch wrapper to object file (not monolithic)
   - Use PCH: `-include-pch build/wasm/wasm_pch.h.pch`
   - Always rebuild sketch (it's small and changes frequently)

2. **Final Linking**:
   - Link: `sketch.o` + `libfastled.a` + `entry_point.o`
   - Use link flags from `build_flags.toml` [linking.base] + [linking.sketch]
   - Output: `fastled.js` + `fastled.wasm`

### Integration Point:
After Phase 2 completes, refactor `ci/wasm_compile_native.py` to use the new library instead of compiling all sources together.

---

## Phase 4: Build Orchestration - ⏳ PENDING

### Objective
Coordinate PCH, library, and sketch compilation intelligently.

### Key Tasks:
1. **Build State Tracking** (`build/wasm/build_state.json`):
   - Store metadata about last build
   - Track PCH hash, library objects, compiler flags, timestamps

2. **Smart Build Algorithm**:
   ```python
   def build_wasm(sketch, output, mode):
       # 1. Check if PCH needs rebuild
       if pch_needs_rebuild():
           build_pch(mode)

       # 2. Check which library objects need rebuild
       changed_sources = detect_changed_library_sources()
       if changed_sources:
           rebuild_library_objects(changed_sources, mode)
           rebuild_library_archive()

       # 3. Always rebuild sketch
       compile_sketch(sketch, mode)

       # 4. Link everything
       link_final(output, mode)
   ```

3. **Performance Instrumentation**:
   - Add timing for each phase
   - Report speedup vs baseline
   - Validate incremental builds working correctly

---

## Performance Targets

### Baseline (Current System):
- **Full build**: ~? seconds (to be measured)
- **Incremental build** (sketch change): ~? seconds (same as full - no caching)

### Target (After All Phases):
- **Full build** (clean): Similar to current (~10-15s acceptable)
- **Incremental build** (sketch only change): **< 2 seconds** (80%+ reduction)
- **Incremental build** (library change, 1 file): **< 5 seconds** (60%+ reduction)

### Phase 1 Contribution:
- **PCH build time**: ~5 seconds (one-time cost)
- **PCH size**: 17MB (well under 50MB target)
- **Savings**: ~986 header dependencies not reparsed on each build

---

## Critical Notes for Next Agent

### What's Working:
1. ✅ PCH infrastructure is complete and tested
2. ✅ `ci/wasm_compile_pch.py` handles smart rebuilds
3. ✅ Dependency file generation working (via `compile_pch.py`)
4. ✅ Metadata tracking working (JSON format)
5. ✅ Build modes supported (debug, fast_debug, quick, release)

### Don't Break These:
- `src/platforms/wasm/compiler/wasm_pch.h` - working PCH header
- `ci/wasm_compile_pch.py` - working PCH script
- `ci/compile_pch.py` - depfile fixing wrapper (reuse for library objects)

### Study Before Starting Phase 2:
1. **Source Discovery**: `ci/meson/rglob.py` - how to find all .cpp files
2. **Current Build**: `ci/wasm_compile_native.py` - how sources are currently compiled
3. **Flag Loading**: `build_flags.toml` - flag configuration system
4. **Meson Reference**: `meson.build` and `examples/meson.build` - static library patterns

### Recommendations:
1. **Start with a simple version**: Get library building first, optimize later
2. **Test incrementally**: Verify each component works before moving to next
3. **Measure everything**: Add timing to validate performance improvements
4. **Use existing patterns**: Reuse `compile_pch.py` for object file depfiles
5. **Parallel compilation**: Use `concurrent.futures.ThreadPoolExecutor` or similar

---

## Implementation Checklist for Phase 2

- [ ] Create `ci/wasm_build_library.py` skeleton
- [ ] Implement source file discovery (reuse `rglob.py`)
- [ ] Implement single object compilation (with PCH)
- [ ] Test single object compilation works
- [ ] Implement dependency file generation for objects
- [ ] Implement parallel compilation of all objects
- [ ] Test parallel compilation works
- [ ] Implement thin archive creation (`emar rcsT`)
- [ ] Test library creation works
- [ ] Implement incremental build logic (check .d files)
- [ ] Test incremental rebuild (modify one source)
- [ ] Test header change detection (modify common header)
- [ ] Test PCH invalidation (all objects rebuild if PCH changes)
- [ ] Measure build times (full vs incremental)
- [ ] Document Phase 2 results

---

## Success Criteria

The implementation is successful when:
1. ✅ **Phase 1**: PCH compiles and invalidates correctly (COMPLETE)
2. ⏳ **Phase 2**: Library builds incrementally, only rebuilding changed objects
3. ⏳ **Phase 3**: Sketch compilation separated, links with library
4. ⏳ **Phase 4**: Full orchestration, < 2s for sketch-only changes

---

## Architecture Overview

```
[Current: Monolithic Build]
sketch.cpp + 251 FastLED sources -> em++ -> fastled.wasm
(~10-15s every time, no caching)

[Target: Incremental Build]
Phase 1: wasm_pch.h -> wasm_pch.h.pch (5s, cached) ✅ COMPLETE
Phase 2: 251 sources + PCH -> 251 *.o -> libfastled.a (incremental)
Phase 3: sketch.cpp + PCH -> sketch.o (always rebuild, ~1s)
Phase 4: sketch.o + libfastled.a + entry_point.o -> fastled.wasm (<2s)
```

---

## Iteration History

### Iteration 1: Phase 1 Implementation ✅
- **Date**: 2025-11-25
- **Agent**: Iteration 1
- **Status**: Complete
- **Files**: See `.agent_task/ITERATION_1.md` for detailed report
- **Key Achievement**: PCH infrastructure working with smart invalidation

---

**Next Agent**: Start Phase 2 - Static Library Build. Good luck! 🚀
