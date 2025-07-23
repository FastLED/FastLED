# Static Analysis Tools for FastLED

FastLED includes comprehensive static analysis tools to help maintain code quality and catch potential issues early in the development process.

## Available Tools

### 1. Include What You Use (IWYU) 🔍

Include What You Use is a tool that analyzes C++ source files and suggests which `#include` statements are needed and which can be removed.

**Benefits:**
- Reduces compilation time by removing unnecessary includes
- Prevents hidden dependencies
- Improves code maintainability
- Catches missing includes that may cause compilation failures

### 2. Clang-Tidy 🔧

Clang-Tidy is a clang-based C++ linter that provides static analysis checks.

**Benefits:**
- Detects potential bugs and code smells
- Enforces coding standards
- Suggests modernization improvements
- Performs static analysis for security issues

## Installation

### Ubuntu/Debian
```bash
sudo apt update
sudo apt install iwyu clang-tidy
```

### macOS
```bash
brew install include-what-you-use llvm
```

### Windows
- Install LLVM from the official website
- Or use vcpkg: `vcpkg install llvm`

### From Source
Visit the official websites for build instructions:
- IWYU: https://include-what-you-use.org/
- Clang-Tidy: https://clang.llvm.org/extra/clang-tidy/

## Usage

### Quick Start - All Analysis Tools

Run comprehensive static analysis on the C++ test suite:

```bash
bash test --check
```

This command:
- Automatically enables `--cpp` and `--clang` modes
- Runs both IWYU and clang-tidy
- Uses the CMake integration for optimal analysis

### IWYU-Specific Commands

**Analyze C++ test suite:**
```bash
uv run ci/ci-iwyu.py
```

**Analyze specific board/platform:**
```bash
# First compile the board
./compile uno --examples Blink

# Then run IWYU analysis
uv run ci/ci-iwyu.py uno
```

**Apply IWYU suggestions automatically:**
```bash
uv run ci/ci-iwyu.py --fix
```

**Verbose output:**
```bash
uv run ci/ci-iwyu.py --verbose
```

**Custom mapping files:**
```bash
uv run ci/ci-iwyu.py --mapping-file custom.imp
```

### Advanced Usage

**Run clang-tidy only:**
```bash
# Build with compile commands
bash test --cpp --clang

# Run clang-tidy manually
bash tidy.sh
```

**CMake Integration:**
```bash
# Enable static analysis during CMake build
cd tests
cmake -B .build -DFASTLED_ENABLE_IWYU=ON -DFASTLED_ENABLE_CLANG_TIDY=ON
cmake --build .build
```

## Configuration

### IWYU Mapping Files

FastLED includes custom mapping files to provide better analysis:

**`ci/iwyu/fastled.imp`** - FastLED-specific mappings
- Maps internal headers to public API
- Prevents suggestions for platform-specific headers
- Defines proper include hierarchies

**`ci/iwyu/stdlib.imp`** - Standard library mappings
- Prefers C++ headers over C headers (`<cstdint>` vs `<stdint.h>`)
- Provides correct symbol-to-header mappings

### Customizing Analysis

**Add custom mapping files:**
```bash
uv run ci/ci-iwyu.py --mapping-file my-custom.imp
```

**Adjust line length limits:**
```bash
uv run ci/ci-iwyu.py --max-line-length 120
```

### CMake Options

Available CMake options for static analysis:

```cmake
-DFASTLED_ENABLE_IWYU=ON           # Enable IWYU
-DFASTLED_ENABLE_CLANG_TIDY=ON     # Enable clang-tidy
```

## Integration with Development Workflow

### Pre-commit Hook

Add to your Git pre-commit hook:
```bash
#!/bin/bash
# Run static analysis before committing
bash test --check
```

### CI/CD Integration

The analysis tools are integrated into the existing CI infrastructure:

```bash
# Part of comprehensive testing
bash test

# Dedicated static analysis
bash tidy.sh
```

### IDE Integration

**VSCode:**
- Install the "clangd" extension
- IWYU suggestions will appear in Problems panel
- Configure `.clangd` for project-specific settings

**CLion:**
- Built-in clang-tidy support
- Configure external tools for IWYU

## Understanding IWYU Output

### Common Suggestions

**Add missing includes:**
```
file.cpp should add these lines:
#include <memory>
```

**Remove unnecessary includes:**
```
file.cpp should remove these lines:
- #include <iostream>  // unused
```

**Use forward declarations:**
```
file.h should add these lines:
class MyClass;

file.h should remove these lines:
- #include "MyClass.h"
```

### Interpreting Messages

- **"should add"** - Missing include for used symbol
- **"should remove"** - Unused include
- **"forward declare"** - Can use forward declaration instead of full include
- **"full type required"** - Forward declaration insufficient, need full include

## Troubleshooting

### Common Issues

**1. "include-what-you-use not found"**
```bash
# Install IWYU (see Installation section above)
which include-what-you-use  # Verify installation
```

**2. "No compile_commands.json found"**
```bash
# Generate compilation database
bash test --cpp --clean
```

**3. "Too many false positives"**
- Check mapping files are being used
- Consider adding project-specific mappings
- Use `--verbose` to understand analysis

**4. "Analysis takes too long"**
- Run on specific files/directories only
- Use faster compiler (clang vs gcc)
- Increase parallel jobs

### Performance Tips

**Speed up analysis:**
- Use clang compiler (`--clang` flag)
- Run on incremental changes only
- Utilize parallel builds
- Cache compilation database

**Reduce noise:**
- Use appropriate mapping files
- Configure exclude patterns for third-party code
- Adjust verbosity levels

## Best Practices

### When to Run Analysis

**Always run:**
- Before committing changes
- After adding new dependencies
- When refactoring include structure

**Regularly run:**
- Weekly on main development branch
- Before releases
- When updating dependencies

### Applying Suggestions

**Review before applying:**
- Not all suggestions are correct
- Consider project-specific patterns
- Test after applying changes

**Incremental approach:**
- Fix one file at a time
- Test after each change
- Commit fixes separately from features

### Team Workflow

**Establish standards:**
- Document project-specific exceptions
- Create team mapping files
- Regular analysis review sessions

**Automation:**
- Integrate into CI/CD pipeline
- Use pre-commit hooks
- Regular automated reports

## Examples

### Basic Analysis Workflow

```bash
# 1. Run analysis
bash test --check

# 2. Review suggestions (manual review)

# 3. Apply fixes (if desired)
uv run ci/ci-iwyu.py --fix

# 4. Test changes
bash test --cpp

# 5. Commit if successful
git add -A
git commit -m "Fix includes based on IWYU analysis"
```

### Platform-Specific Analysis

```bash
# Analyze Arduino Uno platform
./compile uno --examples Blink
uv run ci/ci-iwyu.py uno

# Analyze ESP32 platform  
./compile esp32dev --examples WiFi
uv run ci/ci-iwyu.py esp32dev

# Analyze native test suite
bash test --check
```

### Custom Configuration

```bash
# Create custom mapping file
cat > my-project.imp << 'EOF'
[
  { include: ['"my_internal.h"', 'private', '"my_public.h"', 'public'] }
]
EOF

# Use custom mapping
uv run ci/ci-iwyu.py --mapping-file my-project.imp
```

## Support and Resources

- **IWYU Documentation:** https://include-what-you-use.org/
- **Clang-Tidy Documentation:** https://clang.llvm.org/extra/clang-tidy/
- **FastLED Issues:** https://github.com/FastLED/FastLED/issues
- **FastLED Discussions:** https://github.com/FastLED/FastLED/discussions

For FastLED-specific static analysis questions, please open an issue on the FastLED GitHub repository.