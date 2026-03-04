# Migration Complete: fl/json.h → fl/stl/json.h

## Summary
Successfully migrated `fl/json.h` to `fl/stl/json.h` with detail types in `fl/stl/json/types.h`.

## What was done
1. **New source files** with boost::json-style lowercase naming:
   - `src/fl/stl/json.h` → `fl::json` class
   - `src/fl/stl/json/types.h` → `json_value`, `json_array`, `json_object`
   - `src/fl/stl/json.cpp.hpp` → Implementation

2. **Backward compatibility** maintained via thin wrappers at old locations:
   - `fl/json.h` → `using Json = json;`
   - `fl/json/detail/types.h` → `using JsonValue = json_value;` etc.

3. **All 269 tests pass, all linting passes**

## Key renames
- `Json` → `json`, `JsonValue` → `json_value`, `JsonArray` → `json_array`, `JsonObject` → `json_object`
- Method names normalized to snake_case (old names kept as aliases)
