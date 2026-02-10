# FastLED CI/Build System Agent Guidelines

## 📚 Main Documentation

**For comprehensive guidelines, read these files:**

| Topic | Read |
|-------|------|
| Build commands and restrictions | `docs/agents/build-system.md` |
| Python coding standards | `docs/agents/python-standards.md` |
| Testing commands | `docs/agents/testing-commands.md` |
| MCP server tools | `docs/agents/mcp-tools.md` |
| Debugging strategies | `docs/agents/debugging.md` |

**This file contains only CI-directory-specific guidelines.**

---

## 🔧 Python Code Execution (CI Directory)

### FOREGROUND AGENTS (Interactive Development)

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

### BACKGROUND AGENTS (Automated/CI Environments)

**Restrictions:**
- ❌ **NO tmp.py files** (forbidden for background agents)
- ✅ **Use existing scripts:** `uv run python ci/ci-compile.py uno --examples Blink`
- ✅ **Use MCP server tools** for programmatic operations
- ✅ **Always use `uv run python`** with existing scripts

---

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

---

## Performance Profiling & Optimization

### When to Profile
- Function is performance-critical (hot path in rendering/effects)
- User requests "optimize" or "profile-guided optimization"
- Making algorithmic changes that affect speed
- Comparing performance of different implementations

### Profiling Workflow

**1. Generate Profiler** (if doesn't exist):
```bash
bash profile <function>
# Example: bash profile sincos16
```

This creates `tests/profile/profile_<function>.cpp` - a TEMPLATE that must be customized:
- Add appropriate test inputs for your function
- Ensure benchmark calls target function correctly
- Edit the generated file before building

**2. Customize Profiler:**
Edit `tests/profile/profile_<function>.cpp` to:
- Define realistic test inputs
- Call the target function properly
- Ensure `volatile` prevents optimization

**3. Build & Run Benchmarks:**
```bash
# Local (20 iterations, release mode)
bash profile <function>

# Docker (consistent environment, recommended)
bash profile <function> --docker

# More iterations for better statistics
bash profile <function> --docker --iterations 50

# With callgrind analysis (requires valgrind)
bash profile <function> --docker --callgrind
```

**4. Analyze Results:**
Results are automatically parsed and saved:
- `profile_<function>_results.json` - Raw data (all iterations)
- `profile_<function>_results.ai.json` - Statistical summary

Read the `.ai.json` file for:
- Best/median/worst/stdev timing
- Calls per second
- Performance recommendations

**5. Create Optimization Variant:**
- Implement optimized version (e.g., `sincos16_optimized`)
- Generate profiler for optimized version
- Re-run benchmarks
- Compare results (speedup percentage)

**6. Validate Accuracy:**
- Generate accuracy tests comparing original vs optimized
- Ensure max error is within acceptable tolerance
- Run tests to verify correctness

**7. Report Recommendation:**
- Speedup percentage (e.g., "20% faster")
- Accuracy validation results
- Memory impact (if any)
- Trade-offs (if any)

### Example AI Workflow

```
User: "Optimize sincos16 with profile-guided optimization"

AI Steps:
1. bash profile sincos16 --docker --iterations 30
2. [Reads profile_sincos16_results.ai.json]
   → Median: 47.8 ns/call
3. [Analyzes bottlenecks - maybe via callgrind]
   → LUT bandwidth bottleneck identified
4. [Creates sincos16_optimized with smaller LUT]
5. bash profile sincos16_optimized --docker --iterations 30
6. [Results: 38.1 ns/call]
   → Speedup: 20% faster (47.8 → 38.1 ns/call)
7. [Validates accuracy with generated tests]
   → Max error: 1 (within tolerance)
8. [Recommends: Apply optimization]
   → Performance: 20% faster
   → Accuracy: Preserved
   → Memory: Reduced 50% (1KB → 0.5KB LUT)
```

### Profile Test Architecture

**Directory:** `tests/profile/`
- `profile_<function>.cpp` - Generated profiler (template, needs customization)
- Built as standalone executables (NOT unit tests)
- No doctest, no test runner
- Produce structured JSON output for parsing

**Build Integration:**
- Profile tests excluded from regular test discovery (see `tests/test_config.py`)
- Built via meson with separate `tests/profile/meson.build`
- Compiled with release flags for accurate performance measurement

**Manual Build:**
```bash
# Build without running
uv run test.py profile_<function> --cpp --build-mode release --build

# Run manually
.build/meson-release/tests/profile_<function>.exe baseline
```

**⚠️ Don't use `bash test` for profilers** - profile tests are NOT unit tests and should be run via `bash profile`.

---

## Build Troubleshooting

**For Linker Issues:**
1. Check linker detection and compatibility in test build scripts
2. Review GNU→MSVC flag translation logic
3. Verify warning suppression settings

**For Compiler Issues:**
1. Check compiler detection logic in build scripts
2. Review compiler flags for conflicts
3. Verify optimization settings

**For Build Performance:**
1. Check parallel build configuration
2. Review LTO settings
3. Verify linker selection (mold, lld, default)

---

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

---

## Memory Refresh Rule
**🚨 ALL AGENTS: Read relevant docs/agents/*.md files before concluding CI/build work to refresh memory about current build system rules and requirements.**
