# Regex Test Patterns for intrusive_ptr Migration

## Pattern 1: Remove fl::Referent Inheritance

### Regex Pattern:
```regex
^(\s*)((?:template\s*<[^>]*>\s*)?)(class|struct)(\s+)(\w+)(\s*):(\s*)public(\s+)fl::Referent(\s*)(\{)
```

### Replacement:
```regex
$1$2$3$4$5$10
```

### Test Cases:
```cpp
// Input cases to transform:
class MyClass : public fl::Referent {
    MyClass() {}
};

struct MyStruct : public fl::Referent {
    int value;
};

template <typename T> class MyTemplate : public fl::Referent {
    T data;
};

    class IndentedClass : public fl::Referent {
        void method();
    };
```

### Expected Output:
```cpp
class MyClass {
    MyClass() {}
};

struct MyStruct {
    int value;
};

template <typename T> class MyTemplate {
    T data;
};

    class IndentedClass {
        void method();
    };
```

## Pattern 2: Replace intrusive_ptr with shared_ptr

### Regex Pattern:
```regex
\b(fl::)?intrusive_ptr(<[^>]+>)
```

### Replacement:
```regex
fl::shared_ptr$2
```

### Test Cases:
```cpp
// Input cases to transform:
intrusive_ptr<TestClass> ptr1;
fl::intrusive_ptr<TestClass> ptr2;
intrusive_ptr<CatmullRomParams> make_path(int width, int height);
void setNext(fl::intrusive_ptr<IntrusiveNode> next);
using Storage = Variant<intrusive_ptr<CallableBase>, FreeFunctionCallable>;
template <typename T> struct Hash<intrusive_ptr<T>> {
```

### Expected Output:
```cpp
fl::shared_ptr<TestClass> ptr1;
fl::shared_ptr<TestClass> ptr2;
fl::shared_ptr<CatmullRomParams> make_path(int width, int height);
void setNext(fl::shared_ptr<IntrusiveNode> next);
using Storage = Variant<fl::shared_ptr<CallableBase>, FreeFunctionCallable>;
template <typename T> struct Hash<fl::shared_ptr<T>> {
```

## Pattern 3: Replace make_intrusive with make_shared

### Regex Pattern:
```regex
\b(fl::)?make_intrusive(<[^>]+>)
```

### Replacement:
```regex
fl::make_shared$2
```

### Test Cases:
```cpp
// Input cases to transform:
auto ptr1 = fl::make_intrusive<TestClass>(42);
auto ptr2 = make_intrusive<TestClass>();
intrusive_ptr<CatmullRomParams> params = fl::make_intrusive<CatmullRomParams>();
WaveFxPtr wave_fx = fl::make_intrusive<WaveFx>(xy_rect, args_lower);
mImpl = fl::make_intrusive<VideoImpl>(pixelsPerFrame, fps, frame_history_count);
```

### Expected Output:
```cpp
auto ptr1 = fl::make_shared<TestClass>(42);
auto ptr2 = fl::make_shared<TestClass>();
fl::shared_ptr<CatmullRomParams> params = fl::make_shared<CatmullRomParams>();
WaveFxPtr wave_fx = fl::make_shared<WaveFx>(xy_rect, args_lower);
mImpl = fl::make_shared<VideoImpl>(pixelsPerFrame, fps, frame_history_count);
```

## Files to Exclude from Transformation

### 1. Design Document and Documentation
- `DESIGN_PURGE_INTRUSIVE_PTR.md` - Contains examples and should not be modified
- `*.md` files that document the migration process
- `docs/` directory

### 2. Test Files (Selective)
- `tests/test_memory.cpp` - Contains tests for intrusive_ptr that need careful review
- `tests/test_refptr.cpp` - Tests the old Ptr system, might need special handling

### 3. Legacy/Compatibility Headers
- `src/fl/memory.h` - Will need special handling as it contains compatibility aliases
- `src/fl/ptr.h` - Will be deprecated/removed, not transformed

### 4. Template Specializations
- Files containing `Hash<intrusive_ptr<T>>` specializations need careful review
- Template metaprogramming that depends on intrusive_ptr traits

## Validation Commands

```bash
# Test Pattern 1 - Referent inheritance removal
grep -n "class.*:.*public.*fl::Referent" src/ -R --include="*.h" --include="*.cpp"

# Test Pattern 2 - intrusive_ptr replacement  
grep -n "\bintrusive_ptr<" src/ -R --include="*.h" --include="*.cpp"

# Test Pattern 3 - make_intrusive replacement
grep -n "\bmake_intrusive<" src/ -R --include="*.h" --include="*.cpp"
```