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
- `uv run test.py --no-fingerprint` - Disable fingerprint caching (force rebuild/rerun)
- `bash lint` - Run code formatting/linting
- `uv run ci/ci-compile.py uno --examples Blink` - Compile examples for specific platform
- `uv run ci/wasm_compile.py examples/Blink --just-compile` - Compile Arduino sketches to WASM
- `uv run mcp_server.py` - Start MCP server for advanced tools

### Git Historian (Code Search)
- `/git-historian keyword1 keyword2` - Search codebase and recent git history for keywords
- `/git-historian "error handling" config` - Search for multi-word phrases (use quotes)
- `/git-historian memory leak --paths src tests` - Limit search to specific directories
- Searches both current code AND last 10 commits' diffs in under 4 seconds
- Returns file locations with matching lines + historical context from commits

### QEMU Commands
- `uv run ci/install-qemu.py` - Install QEMU for ESP32 emulation (standalone)
- `uv run test.py --qemu esp32s3` - Run QEMU tests (installs QEMU automatically)
- `FASTLED_QEMU_SKIP_INSTALL=true uv run test.py --qemu esp32s3` - Skip QEMU installation step

### Code Review
- `/code_review` - Run specialized code review checks on changes
  - Reviews src/, examples/, and ci/ changes against project standards
  - Detects try-catch blocks in src/, evaluates new *.ino files for quality
  - Enforces type annotations and KeyboardInterrupt handlers in Python

## Core Rules

### Git and Code Publishing (ALL AGENTS)
- **🚫 NEVER run git commit**: Do NOT create commits - user will commit when ready
- **🚫 NEVER push code to remote**: Do NOT run `git push` or any command that pushes to remote repository
- **User controls all git operations**: All git commit and push decisions are made by the user

### Command Execution (ALL AGENTS)
- **Python**: Always use `uv run python script.py` (never just `python`)
- **Stay in project root** - never `cd` to subdirectories
- **Git-bash compatibility**: Prefix commands with space: `bash test`
- **Platform compilation timeout**: Use minimum 15 minute timeout for platform builds (e.g., `bash compile --docker esp32s3`)

### C++ Code Standards
- **Use `fl::` namespace** instead of `std::`
- **If you want to use a stdlib header like <type_traits>, look check for equivalent in `fl/type_traits.h`
- **Use proper warning macros** from `fl/compiler_control.h`
- **Debug and Warning Output**:
  - **Use `FL_DBG("message" << var)`** for debug prints (easily stripped in release builds)
  - **Use `FL_WARN("message" << var)`** for warnings (persist into release builds)
  - **Avoid `fl::printf`, `fl::print`, `fl::println`** - prefer FL_DBG/FL_WARN macros instead
  - Note: FL_DBG and FL_WARN use stream-style `<<` operator, NOT printf-style formatting
- **Follow existing code patterns** and naming conventions

### Python Code Standards
- **Collection Types**: Always fully type annotate collections using `List[T]`, `Set[T]`, `Dict[K, V]` instead of bare `list`, `set`, `dict` to pass pyright type checking

### JavaScript Code Standards
- **After modifying any JavaScript files**: Always run `bash lint --js` to ensure proper formatting

### Code Review Rule
**🚨 ALL AGENTS: After making code changes, run `/code_review` to validate changes. This ensures compliance with project standards.**

### Memory Refresh Rule
**🚨 ALL AGENTS: Read the relevant AGENTS.md file before concluding work to refresh memory about current project rules and requirements.**

---

*This file intentionally kept minimal. Detailed guidelines are in directory-specific AGENTS.md files.*