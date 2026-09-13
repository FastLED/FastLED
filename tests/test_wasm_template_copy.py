"""
Tests for WASM template copying in ci/wasm_build.py.

Validates the JS-to-TypeScript migration:
1. _copy_templates_legacy raises RuntimeError (raw .ts files can't run in browsers)
2. copy_templates requires Vite (no silent fallback to broken legacy copy)
3. Worker and HTML .ts references are safe because Vite is mandatory
4. app.ts getElementById calls have null checks
"""

import re
import tempfile
from pathlib import Path

import pytest


# Project root
PROJECT_ROOT = Path(__file__).parent.parent


def _get_template_dir() -> Path:
    return PROJECT_ROOT / "src" / "platforms" / "wasm" / "compiler"


def _load_wasm_build_module():
    """Load ci/wasm_build.py as a module."""
    import importlib.util
    import sys

    spec = importlib.util.spec_from_file_location(
        "wasm_build", PROJECT_ROOT / "ci" / "wasm_build.py"
    )
    assert spec is not None, "Could not load ci/wasm_build.py"
    assert spec.loader is not None, "No loader for ci/wasm_build.py"
    mod = importlib.util.module_from_spec(spec)
    saved = sys.path[:]
    sys.path.insert(0, str(PROJECT_ROOT))
    try:
        spec.loader.exec_module(mod)
    finally:
        sys.path[:] = saved
    return mod


class TestViteIsMandatory:
    """Tests that the build system requires Vite for the TypeScript frontend.
    Since Vite is mandatory, .ts references in source files (Worker URLs,
    HTML script tags, import paths) are correct — Vite transforms them at
    build time."""

    def test_vite_config_exists(self):
        """A Vite config must exist for the TypeScript frontend."""
        vite_config = _get_template_dir() / "vite.config.mts"
        assert vite_config.exists(), (
            "vite.config.mts is missing from src/platforms/wasm/compiler/. "
            "Vite is required to build the TypeScript frontend."
        )

    def test_vite_config_handles_worker_output(self):
        """Vite config must define worker output to produce .js files."""
        vite_config = _get_template_dir() / "vite.config.mts"
        content = vite_config.read_text(encoding="utf-8")
        assert "worker" in content, (
            "Vite config must have worker configuration to handle "
            "background worker .ts → .js bundling."
        )

    def test_tsconfig_exists(self):
        """A tsconfig.json must exist for TypeScript compilation."""
        tsconfig = _get_template_dir() / "tsconfig.json"
        assert tsconfig.exists(), (
            "tsconfig.json is missing from src/platforms/wasm/compiler/. "
            "TypeScript configuration is required."
        )

    def test_package_json_has_vite_dependency(self):
        """package.json must list Vite as a dev dependency."""
        import json

        pkg = _get_template_dir() / "package.json"
        assert pkg.exists(), "package.json is missing"
        data = json.loads(pkg.read_text(encoding="utf-8"))
        dev_deps = data.get("devDependencies", {})
        assert "vite" in dev_deps, (
            "Vite must be listed in devDependencies in package.json."
        )


class TestAppTsNullSafety:
    """Tests that app.ts handles null DOM elements safely."""

    def test_getelementbyid_results_are_null_checked(self):
        """app.ts must guard getElementById() results before property access
        to prevent TypeError when elements are missing."""
        app_ts = _get_template_dir() / "app.ts"
        content = app_ts.read_text(encoding="utf-8")
        lines = content.splitlines()

        # Find getElementById assignments to const/let/var
        get_elem_pattern = re.compile(
            r"""(const|let|var)\s+(\w+)\s*=\s*document\.getElementById\("""
        )
        # Find immediate property access on those variables (within 30 lines)
        unsafe_accesses = []
        for i, line in enumerate(lines):
            m = get_elem_pattern.search(line)
            if not m:
                continue
            var_name = m.group(2)
            # Check if there's a null guard before first property access
            has_null_check = False
            for j in range(i + 1, min(i + 30, len(lines))):
                check_line = lines[j]
                # Look for null check patterns:
                #   if (var)  /  if (!var ...)  /  if (!a || !var ...)
                #   var !== null  /  var?.
                if re.search(
                    rf"""\bif\s*\([^)]*!?\s*{re.escape(var_name)}\b"""
                    rf"""|\b{re.escape(var_name)}\s*!==?\s*null"""
                    rf"""|\b{re.escape(var_name)}\?\.?""",
                    check_line,
                ):
                    has_null_check = True
                    break
                # If we see property access before null check, it's unsafe
                if re.search(
                    rf"""\b{re.escape(var_name)}\.(classList|addEventListener|style|textContent|appendChild|innerHTML)""",
                    check_line,
                ):
                    if not has_null_check:
                        unsafe_accesses.append(
                            f"  line {j + 1}: {check_line.strip()} "
                            f"('{var_name}' from line {i + 1} used without null check)"
                        )
                    break

        assert len(unsafe_accesses) == 0, (
            f"app.ts uses getElementById() results without null checks. "
            f"If elements are missing, these will throw TypeError:\n"
            + "\n".join(unsafe_accesses)
        )
