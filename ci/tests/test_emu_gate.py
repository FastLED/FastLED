"""Dependency-free integration tests for the hook/CI emulator selector."""

import asyncio
import json
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from typing import NamedTuple


SCRIPT = Path(__file__).resolve().parents[1] / "emu_gate.py"
MAPPING = """version = 1

[[platforms]]
paths = ["src/platforms/widget/**", "src/platforms/shared/**"]
gates = ["sim:board"]
gap = ""

[[platforms]]
paths = ["src/platforms/shared/**"]
gates = ["sim:other", "sim:board"]
gap = ""

[[platforms]]
paths = ["src/platforms/gap/**"]
gates = []
gap = "no emulator"
"""


class CommandResult(NamedTuple):
    code: int
    stdout: str
    stderr: str


def command(
    args: list[str], cwd: Path, env: dict[str, str] | None = None
) -> CommandResult:
    async def execute() -> CommandResult:
        child = await asyncio.create_subprocess_exec(
            *args,
            cwd=cwd,
            env=env,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await asyncio.wait_for(child.communicate(), 15)
        return CommandResult(child.returncode or 0, stdout.decode(), stderr.decode())

    return asyncio.run(execute())


class EmuGateTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name)
        self.git("init", "-q")
        self.git("config", "user.email", "test@example.invalid")
        self.git("config", "user.name", "Emu Gate Test")
        (self.repo / "ci").mkdir(exist_ok=True)
        (self.repo / "ci" / "emu_map.toml").write_text(MAPPING)
        self.git("add", ".")
        self.git("commit", "-qm", "base")
        self.base = self.git("rev-parse", "HEAD").strip()

    def git(self, *args: str) -> str:
        code, stdout, stderr = command(["git", *args], self.repo)
        self.assertEqual(code, 0, stderr)
        return stdout

    def change(self, *paths: str) -> None:
        for path in paths:
            target = self.repo / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("changed\n")
        self.git("add", ".")
        self.git("commit", "-qm", "change")

    def gate(self, *args: str, env: dict[str, str] | None = None) -> CommandResult:
        return command(
            [
                sys.executable,
                str(SCRIPT),
                "--repo",
                str(self.repo),
                "--base",
                self.base,
                "--head",
                "HEAD",
                *args,
            ],
            Path("/"),
            env,
        )

    def fake_fbuild(self, body: str) -> dict[str, str]:
        binary = self.repo / "bin" / "fbuild"
        binary.parent.mkdir()
        binary.write_text(f"#!{sys.executable}\n{body}\n")
        binary.chmod(0o755)
        return {
            **os.environ,
            "PATH": str(binary.parent) + os.pathsep + os.environ["PATH"],
        }

    def test_mapped_path_check_never_runs_fbuild(self) -> None:
        self.change("src/platforms/widget/driver.h")
        env = self.fake_fbuild("raise SystemExit('must not execute')")
        code, stdout, stderr = self.gate(env=env)
        self.assertEqual((code, stdout, stderr), (0, "sim:board\n", ""))

    def test_shared_mapping_unions_and_deduplicates(self) -> None:
        self.change("src/platforms/shared/driver.h")
        code, stdout, _ = self.gate("--check")
        self.assertEqual(code, 0)
        self.assertEqual(stdout.splitlines(), ["sim:board", "sim:other"])

    def test_gap_and_unknown_paths_are_visible(self) -> None:
        self.change(
            "src/platforms/gap/driver.h", "src/platforms/unknown/driver.h", "README.md"
        )
        code, _, stderr = self.gate()
        self.assertEqual(code, 3)
        self.assertIn("unmapped src/platforms/gap/driver.h (gap: no emulator)", stderr)
        self.assertIn("unmapped src/platforms/unknown/driver.h", stderr)
        # Non-platform files are not this gate's business: listing them buried
        # the real gaps (a platform path nobody mapped) in noise.
        self.assertNotIn("README.md", stderr)

    def test_no_changes_exits_three(self) -> None:
        self.assertEqual(self.gate("--check")[0], 3)

    def test_json_shape_and_mixed_gap(self) -> None:
        self.change("src/platforms/widget/driver.h", "src/platforms/gap/driver.h")
        code, stdout, stderr = self.gate("--json")
        self.assertEqual(code, 0)
        self.assertEqual(
            json.loads(stdout),
            {
                "v": 1,
                "changed": [
                    "src/platforms/gap/driver.h",
                    "src/platforms/widget/driver.h",
                ],
                "required": ["sim:board"],
                "unmapped": [
                    {
                        "path": "src/platforms/gap/driver.h",
                        "platform": "gap",
                        "reason": "no emulator",
                    }
                ],
                "results": [],
            },
        )
        self.assertIn("unmapped", stderr)

    def test_run_failure_and_command_contract(self) -> None:
        self.change("src/platforms/shared/driver.h")
        env = self.fake_fbuild(
            "import sys\n"
            "assert sys.argv[1:6] == ['test-emu', '--emulator', 'sim', '-e', sys.argv[5]]\n"
            "assert sys.argv[6:8] == ['--timeout', '120']\n"
            "assert sys.argv[8].endswith('.build/fbuild/' + sys.argv[5])\n"
            "print('fake emulator output')\n"
            "raise SystemExit(7 if sys.argv[5] == 'board' else 0)"
        )
        code, stdout, stderr = self.gate("--run", "--json", env=env)
        self.assertEqual(code, 1, stderr)
        results = json.loads(stdout)["results"]
        self.assertEqual([r["returncode"] for r in results], [7, 0])
        self.assertTrue(all(r["duration_s"] >= 0 for r in results))
        self.assertEqual(stderr.count("duration="), 2)
        self.assertIn("fake emulator output", stderr)

    def test_timeout_is_failure(self) -> None:
        self.change("src/platforms/widget/driver.h")
        env = self.fake_fbuild("import time\ntime.sleep(10)")
        code, stdout, _ = self.gate("--run", "--json", "--timeout", "0.05", env=env)
        self.assertEqual(code, 1)
        self.assertEqual(json.loads(stdout)["results"][0]["status"], "timeout")

    @unittest.skipUnless(os.name == "posix", "process-group cleanup is POSIX-specific")
    def test_timeout_kills_descendants(self) -> None:
        self.change("src/platforms/widget/driver.h")
        marker = self.repo / "leaked-child"
        env = self.fake_fbuild(
            "import os, time\nfrom pathlib import Path\n"
            "if os.fork() == 0:\n"
            "    time.sleep(0.4)\n"
            f"    Path({str(marker)!r}).write_text('leaked')\n"
            "time.sleep(10)"
        )
        self.assertEqual(self.gate("--run", "--timeout", "0.15", env=env)[0], 1)
        asyncio.run(asyncio.sleep(0.5))
        self.assertFalse(marker.exists())

    def test_run_success(self) -> None:
        self.change("src/platforms/widget/driver.h")
        env = self.fake_fbuild("print('success')")
        code, stdout, stderr = self.gate("--run", "--json", env=env)
        self.assertEqual(code, 0, stderr)
        self.assertEqual(json.loads(stdout)["results"][0]["status"], "passed")

    def test_total_budget_skips_remaining_tests(self) -> None:
        self.change("src/platforms/shared/driver.h")
        env = self.fake_fbuild("import time\ntime.sleep(10)")
        code, stdout, stderr = self.gate("--run", "--json", "--budget", "0.05", env=env)
        self.assertEqual(code, 1, stderr)
        self.assertEqual(
            [r["status"] for r in json.loads(stdout)["results"]],
            ["timeout", "budget_exhausted"],
        )

    def test_missing_fbuild_is_failure(self) -> None:
        self.change("src/platforms/widget/driver.h")
        binary_dir = self.repo / "only-git"
        binary_dir.mkdir()
        (binary_dir / "git").symlink_to(shutil.which("git") or "/usr/bin/git")
        env = {**os.environ, "PATH": str(binary_dir)}
        code, stdout, _ = self.gate("--run", "--json", env=env)
        self.assertEqual(code, 1)
        self.assertEqual(json.loads(stdout)["results"][0]["status"], "error")

    def test_rename_selects_both_old_and_new_platforms(self) -> None:
        self.change("src/platforms/widget/driver.h")
        self.base = self.git("rev-parse", "HEAD").strip()
        self.git("mv", "src/platforms/widget", "src/platforms/gap")
        self.git("commit", "-qm", "rename")
        code, stdout, stderr = self.gate("--json")
        self.assertEqual(code, 0, stderr)
        payload = json.loads(stdout)
        self.assertEqual(payload["required"], ["sim:board"])
        self.assertEqual(
            payload["changed"],
            ["src/platforms/gap/driver.h", "src/platforms/widget/driver.h"],
        )
        self.assertEqual(payload["unmapped"][0]["platform"], "gap")

    def test_empty_mapping_reports_platform_paths_only(self) -> None:
        (self.repo / "ci").mkdir(exist_ok=True)
        (self.repo / "ci" / "emu_map.toml").write_text("version = 1\nplatforms = []\n")
        self.change("src/platforms/widget/driver.h", "docs/readme.md")
        code, stdout, stderr = self.gate("--json")
        self.assertEqual(code, 3, stderr)
        unmapped = json.loads(stdout)["unmapped"]
        self.assertEqual([item["path"] for item in unmapped], ["src/platforms/widget/driver.h"])

    def test_stdlib_only(self) -> None:
        import ast

        for node in ast.walk(ast.parse(SCRIPT.read_text())):
            if isinstance(node, ast.Import):
                self.assertTrue(
                    all(
                        alias.name.split(".")[0] in sys.stdlib_module_names
                        for alias in node.names
                    )
                )
            elif isinstance(node, ast.ImportFrom):
                self.assertIn(
                    (node.module or "").split(".")[0], sys.stdlib_module_names
                )

    def test_top_level_glob_does_not_match_nested_headers(self) -> None:
        (self.repo / "ci" / "emu_map.toml").write_text(
            'version = 1\n[[platforms]]\npaths = ["src/platforms/*.h"]\ngates = ["sim:board"]\n'
        )
        self.change("src/platforms/widget/driver.h")
        self.assertEqual(self.gate()[0], 3)

    def test_deleted_platform_still_requires_test(self) -> None:
        self.change("src/platforms/widget/deep/driver.h")
        self.base = self.git("rev-parse", "HEAD").strip()
        self.git("rm", "src/platforms/widget/deep/driver.h")
        self.git("commit", "-qm", "delete")
        self.assertEqual(self.gate()[:2], (0, "sim:board\n"))

    def test_invalid_ref_is_error(self) -> None:
        self.assertEqual(self.gate("--head", "not-a-ref")[0], 1)

    def test_invalid_mapping_is_error(self) -> None:
        (self.repo / "ci" / "emu_map.toml").write_text('version = 1\nplatforms = ["legacy"]\n')
        self.assertEqual(self.gate()[0], 1)


if __name__ == "__main__":
    unittest.main()
