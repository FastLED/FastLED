# CRGB Namespace Migration Plan

## Overview
Migrate the CRGB class and related color types from global namespace to `fl` namespace, following the established pattern from EOrder and RGBW migrations.

## Current State Analysis

### Files to Review
- `src/crgb.h` - Main CRGB class definition

### Dependencies to Map
- CRGB class methods and operators
- CHSV class and HSV<->RGB conversions
- Color constants (CRGB::Red, CRGB::Blue, etc.)
- Pixel type aliases (e.g., CRGB*, pixel arrays)
- Color utility functions that operate on CRGB/CHSV

## Migration Strategy

### Phase 1: Create Canonical fl Namespace Version
1. **Create `src/fl/crgb.h`**
   - use `cp` to copy the contents of `src/crgb.h` into `src/fl/crgb.h`
   - Move core CRGB class definition to fl namespace
   - Include all operators (+, -, *, /, etc.)
   - Include all color constants (Red, Green, Blue, etc.)
   - Include blend functions and color manipulation methods


### Phase 2: Update Global Headers for Backward Compatibility
1. **Update `src/crgb.h`**
   - Include `fl/crgb.h`
   - Add type alias: `using CRGB = fl::CRGB;`
   - Add global constant aliases for color constants
   - Add global function wrappers where needed

### Phase 3: Handle Special Cases
1. **Color Constants**
   - Ensure `CRGB::Red`, `CRGB::Green`, etc. continue to work
   - May require `using fl::CRGB::Red;` style imports
   - Alternative: Global constants like `const CRGB Red = fl::CRGB::Red;`

2. **Template Functions**
   - Review template functions that accept CRGB parameters
   - Ensure they work with both global and namespaced types
   - May need template specializations or SFINAE

3. **Array and Pointer Types**
   - Ensure `CRGB*`, `CRGB[]` continue to work seamlessly
   - Verify LED strip array declarations remain compatible

### Step 5: Comprehensive Testing
- Run full test suite
- Compile all examples
- Performance verification

## Risk Mitigation

### Potential Issues
1. **Template Compatibility** - Templates expecting global CRGB may need updates
2. **Static Member Access** - `CRGB::Red` style access must continue working
3. **Operator Overloads** - Binary operators between global and namespaced types
4. **Performance** - Aliasing should not introduce overhead

### Mitigation Strategies
1. **Gradual Migration** - Implement and test one component at a time
2. **Comprehensive Testing** - Test both old and new usage patterns
3. **Template Specialization** - Add specializations if needed for compatibility
4. **Performance Monitoring** - Benchmark before and after migration

## Success Criteria
- [ ] All 91 unit tests pass
- [ ] All Arduino examples compile successfully
- [ ] No performance regression
- [ ] Full backward compatibility maintained
- [ ] New fl:: namespace usage works correctly
- [ ] Zero breaking changes for existing user code

## Dependencies
This migration depends on the successful completion of:
- ✅ EOrder namespace migration
- ✅ RGBW namespace migration
- ✅ Build system stability

## Timeline Estimate
- Research and planning: 1-2 hours
- Core implementation: 3-4 hours  
- Testing and validation: 2-3 hours
- Edge case resolution: 1-2 hours

**Total estimated time: 7-11 hours**
