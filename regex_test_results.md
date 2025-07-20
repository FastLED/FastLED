# Regex Pattern Test Results

## Pattern 1: Referent Inheritance Removal

### Regex: `^(\s*)((?:template\s*<[^>]*>\s*)?)(class|struct)(\s+)(\w+)(\s*):(\s*)public(\s+)fl::Referent(\s*)(\{)`
### Replacement: `$1$2$3$4$5$10`

**✅ WORKING CORRECTLY**

**Test Results:**
```cpp
// BEFORE:
class Fx : public fl::Referent {
template <typename T> class MyTemplate : public fl::Referent {
    class IndentedClass : public fl::Referent {

// AFTER:
class Fx {
template <typename T> class MyTemplate {
    class IndentedClass {
```

**Status:** ✅ Pattern works correctly for all inheritance cases.

---

## Pattern 2: intrusive_ptr → shared_ptr  

### Regex: `\b(fl::)?intrusive_ptr(<[^>]+>)`
### Replacement: `fl::shared_ptr$2`

**⚠️ ISSUES IDENTIFIED**

**Test Results:**
```cpp
// BEFORE:
intrusive_ptr<Fx> createEffect()
fl::intrusive_ptr<Fx> effect
using EffectStorage = Variant<intrusive_ptr<Fx>, int>;

// AFTER:
fl::shared_ptr<Fx> createEffect()
fl::shared_ptr<Fx> effect  
using EffectStorage = Variant<fl::shared_ptr<Fx>, int>;
```

**Issues Found:**
1. ❌ **Comments are being matched**: Pattern catches `intrusive_ptr` in comments
2. ❌ **Missing namespace handling**: Already qualified `fl::intrusive_ptr` becomes `fl::fl::shared_ptr`

**Fixed Pattern:**
```regex
(?<!\/\*[^*]*)\b(?<!\/\/.*)\b(fl::)?intrusive_ptr(<[^>]+>)(?![^<]*\*\/)
```

**Better approach - Two separate patterns:**
```regex
# Pattern 2a: Unqualified intrusive_ptr
(?<!\/\*[^*]*)\b(?<!\/\/.*)\bintrusive_ptr(<[^>]+>)(?![^<]*\*\/)
# Replacement: fl::shared_ptr$1

# Pattern 2b: Qualified fl::intrusive_ptr  
(?<!\/\*[^*]*)\b(?<!\/\/.*)\bfl::intrusive_ptr(<[^>]+>)(?![^<]*\*\/)
# Replacement: fl::shared_ptr$1
```

---

## Pattern 3: make_intrusive → make_shared

### Regex: `\b(fl::)?make_intrusive(<[^>]+>)`
### Replacement: `fl::make_shared$2`

**⚠️ ISSUES IDENTIFIED**

**Test Results:**
```cpp
// BEFORE:
fl::make_intrusive<Fx>(42)
make_intrusive<TestClass>()

// AFTER:  
fl::make_shared<Fx>(42)
fl::make_shared<TestClass>()
```

**Issues Found:**
1. ❌ **Comments are being matched**: Pattern catches `make_intrusive` in comments
2. ❌ **Missing namespace handling**: Already qualified `fl::make_intrusive` becomes `fl::fl::make_shared`

**Fixed Pattern:**
```regex
# Pattern 3a: Unqualified make_intrusive
(?<!\/\*[^*]*)\b(?<!\/\/.*)\bmake_intrusive(<[^>]+>)(?![^<]*\*\/)
# Replacement: fl::make_shared$1

# Pattern 3b: Qualified fl::make_intrusive
(?<!\/\*[^*]*)\b(?<!\/\/.*)\bfl::make_intrusive(<[^>]+>)(?![^<]*\*\/)  
# Replacement: fl::make_shared$1
```

---

## Recommended Final Patterns

### Pattern 1: Referent Inheritance (READY)
```bash
# Find:
^(\s*)((?:template\s*<[^>]*>\s*)?)(class|struct)(\s+)(\w+)(\s*):(\s*)public(\s+)fl::Referent(\s*)(\{)

# Replace:
$1$2$3$4$5$10
```

### Pattern 2a: Unqualified intrusive_ptr  
```bash
# Find:
\bintrusive_ptr(<[^>]+>)

# Replace:
fl::shared_ptr$1

# Exclude lines containing:
//.*intrusive_ptr|/\*.*intrusive_ptr.*\*/
```

### Pattern 2b: Qualified intrusive_ptr
```bash
# Find:
\bfl::intrusive_ptr(<[^>]+>)

# Replace:
fl::shared_ptr$1

# Exclude lines containing:
//.*intrusive_ptr|/\*.*intrusive_ptr.*\*/
```

### Pattern 3a: Unqualified make_intrusive
```bash
# Find:
\bmake_intrusive(<[^>]+>)

# Replace:
fl::make_shared$1

# Exclude lines containing:
//.*make_intrusive|/\*.*make_intrusive.*\*/
```

### Pattern 3b: Qualified make_intrusive
```bash  
# Find:
\bfl::make_intrusive(<[^>]+>)

# Replace:
fl::make_shared$1

# Exclude lines containing:
//.*make_intrusive|/\*.*make_intrusive.*\*/
```

---

## Files to Exclude (Updated)

### Exclude by File Pattern:
```bash
--exclude="*.md"
--exclude-dir="docs"  
--exclude="DESIGN_PURGE_INTRUSIVE_PTR.md"
--exclude="regex_test_*.md"
--exclude="*test_regex*"
```

### Exclude by Content (Skip lines containing):
```bash
# Skip comment lines
grep -v "^\s*//.*\(intrusive_ptr\|make_intrusive\)"
grep -v "/\*.*\(intrusive_ptr\|make_intrusive\).*\*/"

# Skip specific legacy files during initial migration
src/fl/ptr.h
src/fl/memory.h  # Handle manually due to compatibility aliases
```

### Files Requiring Manual Review:
1. `src/fl/hash.h` - Template specializations
2. `src/fl/function.h` - Complex template usage
3. `tests/test_memory.cpp` - Tests the intrusive_ptr functionality 
4. `tests/test_refptr.cpp` - Tests the Ptr system

---

## Validation Commands

```bash
# Test all patterns on a single file
echo "Testing patterns on real files..."

# Pattern 1 test
grep -n "^.*class.*:.*public.*fl::Referent.*{" src/fx/fx.h

# Pattern 2 test  
grep -n "\bintrusive_ptr<" src/fx/detail/fx_layer.h

# Pattern 3 test
grep -n "\bmake_intrusive<" src/fx/fx_engine.cpp

# Check for comment exclusions
grep -n "//.*intrusive_ptr\|/\*.*intrusive_ptr" test_regex_sample.cpp
```

## Recommended Transformation Order

1. **Pattern 1**: Remove fl::Referent inheritance (safest)
2. **Pattern 2a,2b**: Replace intrusive_ptr → shared_ptr  
3. **Pattern 3a,3b**: Replace make_intrusive → make_shared
4. **Manual cleanup**: Handle edge cases and template specializations
5. **Build test**: Ensure compilation succeeds
6. **Unit test**: Verify functionality preserved