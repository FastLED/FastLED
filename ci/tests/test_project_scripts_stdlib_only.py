"""The GitHub-project scripts must import only the standard library.

`project_automation.yml` and `project_drift_sync.yml` check out `ci/` with a
bare `actions/setup-python` and install nothing. When the repo banned stdlib
`subprocess`, both scripts were moved onto `running_process`, a package those
jobs do not have, and each died on import with `ModuleNotFoundError` --
`github_project_sync.py` until #4418, `github_project_drift_sync.py` after it,
failing every nightly run. Adding a package install to these secret-bearing
jobs is the wrong trade, so the scripts talk to the GitHub API with `urllib`.

This pins that: every import in each script must resolve to the standard
library. A regression fails here, on every pull request, instead of in a
scheduled job nobody watches.
"""

from __future__ import annotations

import ast
import sys
import unittest
from pathlib import Path


CI_DIR = Path(__file__).resolve().parent.parent

# Scripts run by a workflow that installs no packages.
BARE_PYTHON_SCRIPTS = (
    CI_DIR / "github_project_sync.py",
    CI_DIR / "github_project_drift_sync.py",
)


def top_level_imports(path: Path) -> set[str]:
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    names: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            names.update(alias.name.split(".")[0] for alias in node.names)
        elif isinstance(node, ast.ImportFrom) and node.level == 0 and node.module:
            names.add(node.module.split(".")[0])
    return names


class ProjectScriptsStdlibOnlyTest(unittest.TestCase):
    def test_imports_are_stdlib_only(self: ProjectScriptsStdlibOnlyTest) -> None:
        stdlib = set(sys.stdlib_module_names) | {"__future__"}
        for script in BARE_PYTHON_SCRIPTS:
            with self.subTest(script=script.name):
                third_party = sorted(top_level_imports(script) - stdlib)
                self.assertEqual(
                    third_party,
                    [],
                    f"{script.name} runs under a bare setup-python with no "
                    f"packages installed, but imports {third_party}",
                )


if __name__ == "__main__":
    unittest.main()
