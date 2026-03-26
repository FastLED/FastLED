#!/usr/bin/env python3
"""Get clang-tool-chain runtime DLL paths for Windows test environments.

When zccache caches link results, the ctc-clang++ post-link DLL deployment
is skipped. This script provides the clang-tool-chain runtime directories
so that meson test environments can add them to PATH, ensuring the ASan
shared runtime DLL and other MinGW DLLs are discoverable at test time.

Output: newline-separated list of directories (empty on non-Windows or if
clang-tool-chain is not installed).
"""

import sys


def main() -> None:
    try:
        from clang_tool_chain import get_runtime_dll_paths

        paths = get_runtime_dll_paths()
        for p in paths:
            print(p)
    except ImportError:
        # clang-tool-chain not installed — nothing to output
        pass
    except Exception as e:
        # Non-fatal: don't break meson configure, but log for debugging
        print(f"Warning: get_runtime_dll_paths failed: {e}", file=sys.stderr)


if __name__ == "__main__":
    main()
    sys.exit(0)
