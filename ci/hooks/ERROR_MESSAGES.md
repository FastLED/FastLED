# Forbidden Command Error Messages

This document shows the exact error messages displayed when forbidden commands are blocked by the pre-command hook.

## Command-Specific Error Messages

All error messages now include three parts:
1. **What's forbidden** - The command that was blocked
2. **Why / Alternative** - The recommended alternative or reason
3. **Override instructions** - How to bypass the check if needed

| Command | Error Message |
|---------|--------------|
| `ninja` | ninja is forbidden - use 'bash test' instead (FastLED build system handles ninja invocation). To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 ninja ... |
| `meson` | meson is forbidden - use 'bash test' instead (FastLED build system handles meson configuration). To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 meson ... |
| `clang` | clang is forbidden - use 'bash test' instead (build system uses clang-tool-chain internally). To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 clang ... |
| `clang++` | clang++ is forbidden - use 'bash test' instead (build system uses clang-tool-chain internally). To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 clang++ ... |
| `gcc` | gcc is forbidden - GCC is NOT SUPPORTED by FastLED - project requires Clang 21.1.5. To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 gcc ... |
| `g++` | g++ is forbidden - G++ is NOT SUPPORTED by FastLED - project requires Clang 21.1.5. To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 g++ ... |
| `gdb` | gdb is forbidden - use 'clang-tool-chain-lldb' instead (FastLED's LLDB wrapper). To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 gdb ... |
| `lldb` | lldb is forbidden - use 'clang-tool-chain-lldb' instead (FastLED's LLDB wrapper). To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 lldb ... |
| `pio` | pio is forbidden - use 'bash compile', 'bash debug', or 'bash validate' instead. To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 pio ... |
| `platformio` | platformio is forbidden - use 'bash compile', 'bash debug', or 'bash validate' instead. To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 platformio ... |

## Pattern-Specific Error Messages

| Pattern | Error Message |
|---------|--------------|
| `rm -rf .build*` | rm -rf .build is forbidden - use 'bash test --clean' instead. To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 rm ... |
| `rm -rf .fbuild` | rm -rf .fbuild is forbidden - use 'bash compile --clean' instead. To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 rm ... |
| `--no-fingerprint` | --no-fingerprint is forbidden - makes builds 10-100x slower, use 'bash test --clean' instead. To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 ... |
| `uv run python test.py` | uv run python test.py is forbidden - use 'bash test' or 'uv run test.py' instead. To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 ... |

## Environment Variable Error Messages

| Environment Variable | Error Message |
|---------------------|--------------|
| `SCCACHE_DISABLE=1` | Setting SCCACHE_DISABLE is forbidden (disables critical performance optimization). To override this check, use: FL_AGENT_ALLOW_ALL_CMDS=1 SCCACHE_DISABLE=1 ... |

## Quick Reference by Category

### Build Tools → `bash test`
- `ninja` → Use `bash test`
- `meson` → Use `bash test`

### Compilers → `bash test`
- `clang` → Use `bash test`
- `clang++` → Use `bash test`

### Unsupported Compilers → Not Supported
- `gcc` → ❌ **NOT SUPPORTED** - FastLED requires Clang 21.1.5
- `g++` → ❌ **NOT SUPPORTED** - FastLED requires Clang 21.1.5

### Debuggers → `clang-tool-chain-lldb`
- `gdb` → Use `clang-tool-chain-lldb`
- `lldb` → Use `clang-tool-chain-lldb`

### PlatformIO → `bash compile` / `bash debug` / `bash validate`
- `pio` → Use `bash compile`, `bash debug`, or `bash validate`
- `platformio` → Use `bash compile`, `bash debug`, or `bash validate`

### Cache Management → `--clean` flags
- `rm -rf .build` → Use `bash test --clean`
- `rm -rf .fbuild` → Use `bash compile --clean`

### Performance → Never disable
- `--no-fingerprint` → Use `bash test --clean` (fingerprint cache is reliable)
- `SCCACHE_DISABLE=1` → Never disable sccache (critical performance optimization)

## Override Mechanism

All forbidden commands can be bypassed using the `FL_AGENT_ALLOW_ALL_CMDS=1` environment variable:

**Option 1: Set in command (recommended for AI agents):**
```bash
FL_AGENT_ALLOW_ALL_CMDS=1 ninja --version
FL_AGENT_ALLOW_ALL_CMDS=1 rm -rf .build/meson-quick
FL_AGENT_ALLOW_ALL_CMDS=1 clang++ test.cpp -o test.exe
```

**Option 2: Set in shell environment (for multiple commands):**
```bash
export FL_AGENT_ALLOW_ALL_CMDS=1
ninja --version
clang++ test.cpp -o test.exe
```

**How it works:**
- The hook parses environment variables from the command string (e.g., `VAR=value command`)
- If `FL_AGENT_ALLOW_ALL_CMDS=1` is found in the command, it's allowed
- Otherwise, the hook checks the Claude Code process environment
- If set in either location, the forbidden command is allowed to proceed

**⚠️ Warning:** Only use the override when you have a specific, documented reason to bypass the hook.
