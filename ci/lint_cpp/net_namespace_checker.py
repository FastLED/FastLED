#!/usr/bin/env python3
"""Backward-compatible re-export — real implementation in subdir_namespace_checker.py."""

from ci.lint_cpp.subdir_namespace_checker import (
    NetNamespaceChecker,  # noqa: F401
    SubdirNamespaceChecker,  # noqa: F401
)


__all__ = ["NetNamespaceChecker", "SubdirNamespaceChecker"]
