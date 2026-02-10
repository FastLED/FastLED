# FastLED Pre-Command Hook

## Overview

This directory contains pre-command hooks that prevent Claude AI agents from running dangerous or forbidden build commands that could bypass FastLED's build system safety checks.

## check_forbidden_commands.py

**Purpose:** Blocks direct invocation of build system commands that should go through FastLED's bash wrapper scripts.

**Location:** `ci/hooks/check_forbidden_commands.py`
**Hook Type:** `PreToolUse` (runs before any Bash command)
**Configuration:** `.claude/settings.json`

### Forbidden Commands and Recommended Alternatives

The hook blocks these commands to prevent bypassing the build system. Use the recommended alternatives instead:

| Forbidden Command | Category | Recommended Alternative | Notes |
|-------------------|----------|------------------------|-------|
| `ninja` | Build Tool | `bash test` | FastLED build system handles ninja invocation |
| `meson` | Build Tool | `bash test` | FastLED build system handles meson configuration |
| `clang` | Compiler | `bash test` | Build system uses clang-tool-chain internally |
| `clang++` | Compiler | `bash test` | Build system uses clang-tool-chain internally |
| `gcc` | Compiler | ❌ **Not supported** | FastLED requires Clang 21.1.5 - GCC is not supported |
| `g++` | Compiler | ❌ **Not supported** | FastLED requires Clang 21.1.5 - GCC is not supported |
| `gdb` | Debugger | `clang-tool-chain-lldb` | Use FastLED's LLDB wrapper for debugging |
| `lldb` | Debugger | `clang-tool-chain-lldb` | Use FastLED's LLDB wrapper for debugging |
| `pio` | PlatformIO | `bash compile`, `bash debug`, `bash validate` | Use FastLED's platform compilation wrappers |
| `platformio` | PlatformIO | `bash compile`, `bash debug`, `bash validate` | Use FastLED's platform compilation wrappers |

**Key Points:**
- ✅ **Build Tools (ninja/meson)**: Always use `bash test` - the build system manages these internally
- ✅ **Compilers (clang/clang++)**: Always use `bash test` - the build system uses clang-tool-chain wrappers
- ❌ **GCC/G++**: Not supported by FastLED - project requires Clang 21.1.5 for MSVC compatibility
- 🔍 **Debuggers (gdb/lldb)**: Use `clang-tool-chain-lldb` for consistent debugging experience
- 🔌 **PlatformIO**: Use `bash compile` (build), `bash debug` (interactive debugging), or `bash validate` (hardware testing)

### Forbidden Patterns

The hook also blocks dangerous patterns:

#### Cache Deletion
- `rm -rf .build` - Use `bash test --clean` instead
- `rm -rf .fbuild` - Use `bash compile --clean` instead

**Rationale:** Build system is self-healing and has special cache invalidation code. Manual deletion can corrupt build state.

#### Fingerprint Caching
- `--no-fingerprint` - Use `bash test --clean` instead

**Rationale:** Makes builds 10-100x slower. Fingerprint cache is reliable and tracks changes correctly.

#### Environment Variables
- `SCCACHE_DISABLE=1` - Never disable sccache

**Rationale:** sccache provides critical compilation performance optimization.

### Override Mechanism

To bypass the hook for legitimate reasons, set the environment variable:

```bash
FL_AGENT_ALLOW_ALL_CMDS=1 ninja --version
FL_AGENT_ALLOW_ALL_CMDS=1 rm -rf .build/meson-quick
```

**Warning:** Only use the override when you have a specific, documented reason to run these commands directly.

### Testing

The hook has been tested with:

✅ All forbidden commands (ninja, meson, clang, clang++, gcc, g++, gdb, lldb, pio, platformio)
✅ All forbidden patterns (cache deletion, --no-fingerprint, SCCACHE_DISABLE)
✅ Environment variable override works
✅ Normal commands pass through

### Error Messages

When blocked, you'll see clear error messages:

```
ninja is forbidden. If you really want to run this manually, set environment variable "FL_AGENT_ALLOW_ALL_CMDS"

rm -rf .build is forbidden - use 'bash test --clean' instead

--no-fingerprint is forbidden - makes builds 10-100x slower, use 'bash test --clean' instead

Setting SCCACHE_DISABLE is forbidden. If you really want to set this, use environment variable "FL_AGENT_ALLOW_ALL_CMDS"
```

### Architecture

**Hook Flow:**
1. Claude attempts Bash command
2. Hook receives JSON input via stdin
3. Hook checks command against forbidden lists
4. Hook checks for `FL_AGENT_ALLOW_ALL_CMDS` override
5. Hook exits with:
   - Exit 0: Allow command
   - Exit 2: Block command (error message to Claude)

**Key Functions:**
- `is_forbidden_command()` - Checks command starts with forbidden binary
- `check_forbidden_pattern()` - Checks command matches forbidden regex
- `check_forbidden_env_vars()` - Checks command sets forbidden env vars

### Adding New Forbidden Commands

To add more forbidden commands, edit `ci/hooks/check_forbidden_commands.py`:

```python
FORBIDDEN_COMMANDS = [
    # Add new command here
    "new_forbidden_cmd",
]

FORBIDDEN_PATTERNS = [
    # Add new pattern here
    (r"pattern_regex", "Error message to show"),
]

FORBIDDEN_ENV_VARS = [
    # Add new env var here
    "NEW_FORBIDDEN_VAR",
]
```

### Debugging

To see hook execution in detail:
- Run Claude Code with `--debug` flag
- Toggle verbose mode with `Ctrl+O` during session
- Check `.claude/settings.json` for hook configuration

### Related Documentation

- `docs/agents/build-system.md` - Build system standards
- `CLAUDE.md` - Core agent rules
- `docs/agents/testing-commands.md` - Testing commands
