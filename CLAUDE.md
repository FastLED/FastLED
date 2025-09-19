# FastLED AI Agent Guidelines

## Quick Reference

This project uses directory-specific agent guidelines. See:

- **CI/Build Tasks**: `ci/AGENTS.md` - Python build system, compilation, MCP server tools
- **Testing**: `tests/AGENTS.md` - Unit tests, test execution, validation requirements  
- **Examples**: `examples/AGENTS.md` - Arduino sketch compilation, .ino file rules

## Key Commands

- `uv run test.py` - Run all tests
- `uv run test.py --cpp` - Run C++ tests only
- `uv run test.py TestName` - Run specific C++ test (e.g., `uv run test.py xypath`)
- `bash lint` - Run code formatting/linting
- `uv run ci/ci-compile.py uno --examples Blink` - Compile examples for specific platform
- `uv run ci/wasm_compile.py examples/Blink --just-compile` - Compile Arduino sketches to WASM
- `uv run mcp_server.py` - Start MCP server for advanced tools

### QEMU Commands
- `uv run ci/install-qemu.py` - Install QEMU for ESP32 emulation (standalone)
- `uv run test.py --qemu esp32s3` - Run QEMU tests (installs QEMU automatically)
- `FASTLED_QEMU_SKIP_INSTALL=true uv run test.py --qemu esp32s3` - Skip QEMU installation step

## Core Rules

### Command Execution (ALL AGENTS)
- **Python**: Always use `uv run python script.py` (never just `python`)
- **Stay in project root** - never `cd` to subdirectories
- **Git-bash compatibility**: Prefix commands with space: `bash test`

### C++ Code Standards
- **Use `fl::` namespace** instead of `std::`
- **If you want to use a stdlib header like <type_traits>, look check for equivalent in `fl/type_traits.h`
- **Use proper warning macros** from `fl/compiler_control.h`
- **Follow existing code patterns** and naming conventions

### JavaScript Code Standards
- **After modifying any JavaScript files**: Always run `bash lint --js` to ensure proper formatting

### Memory Refresh Rule
**🚨 ALL AGENTS: Read the relevant AGENTS.md file before concluding work to refresh memory about current project rules and requirements.**

---

*This file intentionally kept minimal. Detailed guidelines are in directory-specific AGENTS.md files.*