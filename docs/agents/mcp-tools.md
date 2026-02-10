# MCP Server Tools

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

## Platform Build Information Analysis

**Generate Build Info:**
```bash
# Compile a platform to generate build_info.json (Blink is compiled by default)
uv run ci/ci-compile.py uno
uv run ci/ci-compile.py esp32dev
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

## Stack Trace Setup

The FastLED project supports enhanced debugging through stack trace functionality for crash analysis and debugging.

**For Background Agents**: Use the MCP server tool `setup_stack_traces` to automatically install and configure stack trace support:

```bash
# Via MCP server (recommended for background agents)
uv run mcp_server.py
# Then use setup_stack_traces tool with method: "auto"
```

**Manual Installation**:

**Ubuntu/Debian**:
```bash
sudo apt-get update
sudo apt-get install -y libunwind-dev build-essential
```

**CentOS/RHEL/Fedora**:
```bash
sudo yum install -y libunwind-devel  # CentOS/RHEL
sudo dnf install -y libunwind-devel  # Fedora
```

**macOS**:
```bash
brew install libunwind
```

### Available Stack Trace Methods
1. **LibUnwind** (Recommended) - Enhanced stack traces with symbol resolution
2. **Execinfo** (Fallback) - Basic stack traces using standard glibc
3. **Windows** (On Windows) - Windows-specific debugging APIs
4. **No-op** (Last resort) - Minimal crash handling

The build system automatically detects and configures the best available option.

### Testing Stack Traces
Stack trace testing is integrated into the main test suite. Use the crash handler tests via the standard test runner:

```bash
bash test crash_test  # Run crash handler tests
```

### Using in Code
```cpp
#include "tests/crash_handler.h"

int main() {
    setup_crash_handler();  // Enable crash handling
    // Your code here...
    return 0;
}
```

## 🚨 CRITICAL: Background Agent Validation

**ALL BACKGROUND AGENTS MUST USE `validate_completion` TOOL BEFORE INDICATING COMPLETION:**

1. **🚨 ALWAYS RUN `validate_completion` VIA MCP SERVER**
   - This is MANDATORY and NON-NEGOTIABLE for all background agents
   - The tool runs `bash test` and validates that all tests pass
   - Background agents MUST NOT indicate they are "done" until ALL tests pass

2. **🚨 ZERO TOLERANCE FOR TEST FAILURES**
   - If ANY test fails, the background agent MUST fix the issues before completion
   - Do NOT indicate completion with failing tests
   - Do NOT ignore test errors or warnings

### Background Agent Completion Checklist:
- [ ] All code changes have been made
- [ ] MCP server `validate_completion` tool shows success
- [ ] No compilation errors or warnings
- [ ] Only then indicate task completion

**FAILURE TO FOLLOW THESE REQUIREMENTS WILL RESULT IN BROKEN CODE SUBMISSIONS.**
