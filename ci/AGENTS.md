# FastLED CI/Build System Agent Guidelines

## 🔧 Python Build System

### Core Architecture
FastLED uses a high-performance Python-based build system:

**Key Components:**
- `ci/compiler/clang_compiler.py` - Core compiler with PCH and fingerprint cache
- `ci/ci/fingerprint_cache.py` - 99% faster PCH rebuilds through intelligent caching
- `ci/util/build_flags.py` - TOML-based build configuration management
- `ci/compiler/test_example_compilation.py` - Example compilation system

**Performance Features:**
- **PCH (Precompiled Headers)** - Dramatically faster compilation
- **Fingerprint Cache** - 99% time reduction for unchanged dependencies
- **Parallel Compilation** - Automatic CPU-optimized parallel builds
- **Smart Rebuilds** - Only rebuild when source files actually change

### 🚨 Command Execution Rules

#### FOREGROUND AGENTS (Interactive Development)
**Python Code Execution:**
- ✅ **Create/modify tmp.py** in project root (NOT subdirectories)
- ✅ **Always run: `uv run python tmp.py`** (never just `python`)
- ✅ **Correct import paths:** `from ci.ci.module` for ci.ci modules, `from ci.module` for top-level ci modules
- ❌ **NEVER cd to subdirectories** - stay in project root

**Example:**
```python
# tmp.py (created in project root)
from ci.ci.clang_compiler import Compiler  # CORRECT import path
import subprocess
result = subprocess.run(['git', 'status'], capture_output=True, text=True)
print(result.stdout)
```
Then run: `uv run python tmp.py`

#### BACKGROUND AGENTS (Automated/CI Environments)
**Restrictions:**
- ❌ **NO tmp.py files** (forbidden for background agents)
- ✅ **Use existing scripts:** `uv run python ci/ci-compile.py uno --examples Blink`
- ✅ **Use MCP server tools** for programmatic operations
- ✅ **Always use `uv run python`** with existing scripts

## 🤖 MCP Server Configuration

**Start server:** `uv run mcp_server.py`

**Available Tools:**
- Running tests with various options
- Compiling examples for different platforms
- Code fingerprinting and change detection
- Linting and formatting
- **Build info analysis** for platform-specific defines, compiler flags, toolchain information
- **Symbol analysis** for binary optimization (all platforms)
- Stack trace setup for enhanced debugging
- **🌐 FastLED Web Compiler** with Playwright (FOREGROUND AGENTS ONLY)
- **🚨 `validate_completion` tool** (MANDATORY for background agents)

### FastLED Web Compiler (FOREGROUND AGENTS ONLY)

**Prerequisites:**
- `pip install fastled` - FastLED web compiler
- `pip install playwright` - Browser automation
- Docker (optional, for faster compilation)

**Usage via MCP Server:**
```python
# Basic usage
use run_fastled_web_compiler tool with:
- example_path: "examples/Audio"
- capture_duration: 30
- headless: false
- save_screenshot: true
```

**Key Features:**
- Compiles Arduino sketches to WASM for browser execution
- Captures console.log output with playwright automation
- Takes screenshots of running visualizations
- Monitors FastLED_onFrame calls to verify proper initialization

**🚫 BACKGROUND AGENT RESTRICTION:** This tool is COMPLETELY DISABLED for background agents and CI environments.

## 🤖 Linting Guidelines

### FOREGROUND AGENTS
**🚨 ALWAYS USE `bash lint` - DO NOT RUN INDIVIDUAL LINTING SCRIPTS**

- ✅ **CORRECT:** `bash lint`
- ❌ **FORBIDDEN:** `./lint-js`, `./check-js`, `python3 scripts/enhance-js-typing.py`
- ❌ **FORBIDDEN:** `uv run ruff check`, `uv run black`, individual tools

**WHY:** `bash lint` provides comprehensive coverage of Python, C++, JavaScript, and enhancement analysis.

### BACKGROUND AGENTS
**CAN USE FINE-GRAINED LINTING FOR SPECIFIC NEEDS**

Background agents may use individual linting scripts when needed:
- `./lint-js` - JavaScript-only linting
- `./check-js` - JavaScript type checking
- `python3 scripts/enhance-js-typing.py` - JavaScript enhancement analysis
- `uv run ruff check` - Python linting only
- MCP server `lint_code` tool - Programmatic access

**BUT STILL PREFER `bash lint` FOR COMPREHENSIVE CHECKING**

## Build System Commands

**Unit Test Compilation:**
```bash
# Fast Python API (default)
bash test --unit --verbose  # Uses PCH optimization

# Disable PCH if needed  
bash test --unit --no-pch --verbose

# Legacy CMake system (8x slower)
bash test --unit --legacy --verbose  
```

**Available Build Options:**
- `--no-pch` - Disable precompiled headers 
- `--clang` - Use Clang compiler (recommended for speed)
- `--clean` - Force full rebuild
- `--verbose` - Show detailed compilation output
- `--legacy` - Use old CMake system (discouraged)

**Platform Compilation:**
```bash
# Compile examples for specific platforms
uv run ci/ci-compile.py uno --examples Blink
uv run ci/ci-compile.py esp32dev --examples Blink
uv run ci/ci-compile.py teensy31 --examples Blink
```

**WASM Compilation:**
```bash
# Compile any sketch directory to WASM
uv run ci/wasm_compile.py path/to/your/sketch

# Quick compile test (compile only, no browser)
uv run ci/wasm_compile.py examples/Blink --just-compile

# Compile and open browser
uv run ci/wasm_compile.py examples/Blink -b --open
```

## Build Troubleshooting

**For Linker Issues:**
1. Check `tests/cmake/LinkerCompatibility.cmake` first
2. Look for lld-link detection and compatibility functions
3. Check GNU→MSVC flag translation logic
4. Verify warning suppression settings

**For Compiler Issues:**
1. Check `tests/cmake/CompilerDetection.cmake` for detection logic
2. Review `tests/cmake/CompilerFlags.cmake` for flag conflicts
3. Verify optimization settings in `OptimizationSettings.cmake`

**For Build Performance:**
1. Check `tests/cmake/ParallelBuild.cmake` for parallel settings
2. Review LTO configuration in `OptimizationSettings.cmake`
3. Verify linker selection (mold, lld, default)

## 🚨 Critical User Feedback Corrections

**Always Use `uv run python` - NEVER just `python` or `uv run`:**
- ❌ **WRONG**: `python -c "..."`
- ❌ **WRONG**: `uv run -c "..."`  
- ✅ **CORRECT**: `uv run python -c "..."`

**Correct Import Paths:**
- ✅ **CORRECT**: `from ci.ci.clang_compiler import ...` (for ci.ci modules)
- ✅ **CORRECT**: `from ci.clang_compiler import ...` (for top-level ci modules, if they exist)

**Never Change Directory:**
- ❌ **WRONG**: `cd ci && python ...`
- ❌ **WRONG**: `cd ci/ci && python ...`
- ✅ **CORRECT**: Stay in project root, use full paths: `uv run python ci/script.py`

**Git-Bash Terminal Compatibility:**
Use leading space for git-bash compatibility:
- ✅ **CORRECT**: ` bash test` (note the leading space)
- ❌ **INCORRECT**: `bash test` (may get truncated to `ash test`)

## Platform Build Information Analysis

**Generate Build Info:**
```bash
# Compile a platform to generate build_info.json
uv run ci/ci-compile.py uno --examples Blink
uv run ci/ci-compile.py esp32dev --examples Blink
```

**Analyze Platform Defines:**
```bash
# Command line tool
python3 ci/ci/build_info_analyzer.py --board uno --show-defines

# Via MCP server
uv run mcp_server.py
# Use build_info_analysis tool with: board="uno", show_defines=true
```

**Compare Platforms:**
```bash
# Command line
python3 ci/ci/build_info_analyzer.py --compare uno esp32dev

# Via MCP 
# Use build_info_analysis tool with: board="uno", compare_with="esp32dev"
```

## Symbol Analysis for Binary Optimization

**Analyze specific platform:**
```bash
uv run ci/ci/symbol_analysis.py --board uno
uv run ci/ci/symbol_analysis.py --board esp32dev
uv run ci/ci/symbol_analysis.py --board teensy31
```

**Analyze all available platforms:**
```bash
uv run ci/demo_symbol_analysis.py
```

**Using MCP Server:**
```bash
# Use MCP server tools
uv run mcp_server.py
# Then use symbol_analysis tool with board: "uno" or "auto"
# Or use symbol_analysis tool with run_all_platforms: true
```

**JSON Output:**
```bash
uv run symbol_analysis.py --board esp32dev --output-json
# Results saved to: .build/esp32dev_symbol_analysis.json
```

## Memory Refresh Rule
**🚨 ALL AGENTS: Read ci/AGENTS.md before concluding CI/build work to refresh memory about current Python build system rules and MCP server requirements.**
