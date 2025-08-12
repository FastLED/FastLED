### Compile Commands Generator

- **Script**: `ci/compile-commands.py`
- **Purpose**: Generate a correct `compile_commands.json` for clangd/clang-tidy based on the exact flags used to build `libfastled.a` (no longer piggy-backing on unit-test builds).

#### Usage
```bash
uv run ci/compile-commands.py
# Optional:
#   --output /workspace/compile_commands.json   # default is project root
#   --clean                                     # rebuild command set from scratch
#   --verbose                                   # print collected flags/files
```

#### Inputs and configuration
- The script reads its build configuration from `ci/build_commands.toml`.
- If this file does not exist, the script will write an initial template to `ci/build_commands.toml` and then proceed using those defaults.

Suggested shape of `ci/build_commands.toml` (minimal):
```toml
[tools]
cpp_compiler = ["uv", "run", "python", "-m", "ziglang", "c++"]

[flags]
compiler_flags = ["-std=gnu++17", "-fpermissive", "-Wall", "-Wextra"]
defines = ["STUB_PLATFORM"]
include_paths = ["src", "src/platforms/stub"]

[filters]
exclude_dirs = ["src/platforms/esp", "src/platforms/avr", "src/platforms/arm", "src/platforms/wasm"]
```

#### Behavior
- Enumerates the source files that participate in `libfastled.a` (respecting `filters.exclude_dirs`).
- Emits one entry per TU with the exact command line constructed from `[tools]`, `[flags]`, and resolved include paths.
- Writes `compile_commands.json` at the project root by default (configurable via `--output`).

#### Rationale
- Avoids stale or incorrect flags from unit-test builds.
- Provides a single, explicit source of truth for IntelliSense and static analysis.