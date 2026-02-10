# Forbidden Command Error Messages

This document shows the exact error messages displayed when forbidden commands are blocked by the pre-command hook.

## Command-Specific Error Messages

| Command | Error Message |
|---------|--------------|
| `ninja` | ninja is forbidden - use 'bash test' instead (FastLED build system handles ninja invocation) |
| `meson` | meson is forbidden - use 'bash test' instead (FastLED build system handles meson configuration) |
| `clang` | clang is forbidden - use 'bash test' instead (build system uses clang-tool-chain internally) |
| `clang++` | clang++ is forbidden - use 'bash test' instead (build system uses clang-tool-chain internally) |
| `gcc` | gcc is forbidden - GCC is NOT SUPPORTED by FastLED - project requires Clang 21.1.5 |
| `g++` | g++ is forbidden - G++ is NOT SUPPORTED by FastLED - project requires Clang 21.1.5 |
| `gdb` | gdb is forbidden - use 'clang-tool-chain-lldb' instead (FastLED's LLDB wrapper) |
| `lldb` | lldb is forbidden - use 'clang-tool-chain-lldb' instead (FastLED's LLDB wrapper) |
| `pio` | pio is forbidden - use 'bash compile', 'bash debug', or 'bash validate' instead |
| `platformio` | platformio is forbidden - use 'bash compile', 'bash debug', or 'bash validate' instead |

## Pattern-Specific Error Messages

| Pattern | Error Message |
|---------|--------------|
| `rm -rf .build*` | rm -rf .build is forbidden - use 'bash test --clean' instead |
| `rm -rf .fbuild` | rm -rf .fbuild is forbidden - use 'bash compile --clean' instead |
| `--no-fingerprint` | --no-fingerprint is forbidden - makes builds 10-100x slower, use 'bash test --clean' instead |
| `SCCACHE_DISABLE=1` | Setting SCCACHE_DISABLE is forbidden. If you really want to set this, use environment variable "FL_AGENT_ALLOW_ALL_CMDS" |

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

All forbidden commands can be bypassed using:

```bash
FL_AGENT_ALLOW_ALL_CMDS=1 <forbidden_command>
```

**Example:**
```bash
FL_AGENT_ALLOW_ALL_CMDS=1 ninja --version
FL_AGENT_ALLOW_ALL_CMDS=1 rm -rf .build/meson-quick
```

**⚠️ Warning:** Only use the override when you have a specific, documented reason to bypass the hook.
