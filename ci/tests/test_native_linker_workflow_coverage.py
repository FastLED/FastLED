"""Every Linux host-native Meson caller must select the pinned Wild linker.

FastLED #4558: #4552 installed upstream Wild in the reusable native unit and
example templates, but the MP3 audit workflows and ``linux_native`` ran
``bash test`` / ``bash profile`` without it and silently kept LLD. This guard
fails when a workflow job that can run on Linux executes a host-native Meson
build step without an earlier ``ci/tools/install_native_linker.py`` step in
the same job.

Host-native Meson steps are ``bash test`` (except Python-only ``--py`` runs),
``bash profile``, ``test.py`` and ``ci/profile_runner.py``. Steps pinned to
``--build-mode release`` are exempt: release links ThinLTO bitcode, which the
pinned Wild cannot consume, so ``resolve_native_linker`` keeps LLD there. MCU
cross-compilation (``bash compile``, fbuild) is deliberately out of scope.
"""

import re
import unittest
from pathlib import Path
from typing import Any

import yaml


WORKFLOWS = Path(__file__).resolve().parents[2] / ".github" / "workflows"
INSTALLER = "ci/tools/install_native_linker.py"
NATIVE_STEP = re.compile(
    r"(?:^|[\s;&|(])(?:bash\s+test|bash\s+profile|(?:uv\s+run\s+(?:python\s+)?)?test\.py|ci/profile_runner\.py)\b[^\n]*"
)


def _is_native_meson(command: str) -> bool:
    for match in NATIVE_STEP.finditer(command):
        line = match.group(0)
        if "bash test" in line and "--py" in line and "--cpp" not in line:
            continue
        if "--build-mode release" in line:
            continue  # ThinLTO release links always keep LLD (#4558)
        return True
    return False


def _may_run_on_linux(job: dict[str, Any]) -> bool:
    runs_on = job.get("runs-on")
    if runs_on is None:
        return False  # reusable-workflow caller; the template is checked itself
    text = str(runs_on)
    if "${{" in text:
        return True  # chosen at dispatch/matrix time: assume Linux is possible
    return "ubuntu" in text or "linux" in text.lower()


def uncovered_native_steps(workflow: dict[str, Any]) -> list[str]:
    """Names of Linux native Meson steps that run before any Wild install."""
    missing: list[str] = []
    for job_name, job in (workflow.get("jobs") or {}).items():
        if not isinstance(job, dict) or not _may_run_on_linux(job):
            continue
        installed = False
        for step in job.get("steps") or []:
            run = str(step.get("run", ""))
            if INSTALLER in run:
                installed = True
                continue
            if _is_native_meson(run) and not installed:
                missing.append(f"{job_name}: {step.get('name', run.strip()[:60])}")
    return missing


class NativeLinkerWorkflowCoverageTests(unittest.TestCase):
    def test_every_linux_native_meson_step_installs_wild_first(self) -> None:
        problems: list[str] = []
        for path in sorted(WORKFLOWS.glob("*.yml")):
            workflow = yaml.safe_load(path.read_text(encoding="utf-8")) or {}
            for step in uncovered_native_steps(workflow):
                problems.append(f"{path.name} -> {step}")
        self.assertEqual(
            problems,
            [],
            "Linux host-native Meson steps must run after "
            f"`uv run python {INSTALLER}` in the same job (FastLED #4558)",
        )

    def test_guard_flags_a_bypassing_job_and_accepts_the_covered_shape(self) -> None:
        bypass = {
            "jobs": {
                "audit": {
                    "runs-on": "ubuntu-24.04",
                    "steps": [
                        {"name": "Install", "run": "uv sync"},
                        {"name": "Codec", "run": "bash test fl_codec --cpp"},
                        {"name": "Profile", "run": "bash profile x --iterations 1"},
                        {"name": "Python only", "run": "bash test --py --verbose"},
                        {
                            "name": "LTO",
                            "run": "bash test x --cpp --build-mode release",
                        },
                    ],
                }
            }
        }
        self.assertEqual(
            uncovered_native_steps(bypass), ["audit: Codec", "audit: Profile"]
        )
        covered = {
            "jobs": {
                "audit": {
                    "runs-on": "ubuntu-24.04",
                    "steps": [
                        {"run": f"uv run python {INSTALLER}"},
                        {"name": "Codec", "run": "bash test fl_codec --cpp"},
                    ],
                },
                "mac": {
                    "runs-on": "macos-14",
                    "steps": [{"name": "Codec", "run": "bash test --cpp"}],
                },
                "boards": {
                    "runs-on": "ubuntu-24.04",
                    "steps": [
                        {"name": "MCU", "run": "bash compile esp32s3 --examples Blink"}
                    ],
                },
            }
        }
        self.assertEqual(uncovered_native_steps(covered), [])


if __name__ == "__main__":
    unittest.main()
