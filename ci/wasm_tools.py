#!/usr/bin/env python3
"""
Utility functions for accessing WASM compilation tools.

All tools are accessed via clang-tool-chain wrappers which handle
platform detection, path resolution, and environment setup automatically.
"""


def get_wasm_ld() -> str:
    """
    Get wasm-ld linker command.

    Returns:
        Command name for wasm-ld (uses clang-tool-chain wrapper)
    """
    return "clang-tool-chain-wasm-ld"


def get_emcc() -> str:
    """
    Get emscripten C compiler command.

    Returns:
        Command name for emcc (uses clang-tool-chain wrapper)
    """
    return "clang-tool-chain-emcc"


def get_emar() -> str:
    """
    Get emscripten archiver command.

    Returns:
        Command name for emar (uses clang-tool-chain wrapper)
    """
    return "clang-tool-chain-emar"
