import os
import subprocess
import sys
from pathlib import Path

MINIMUM_REPORT_SEVERTIY = "medium"
MINIMUM_FAIL_SEVERTIY = "high"


def main() -> int:
    here = Path(__file__).parent
    project_root = here.parent
    build = project_root / ".build"
    os.chdir(str(build))

    # Change to the first subdirectory in .build
    subdirs = [d for d in os.listdir() if os.path.isdir(d)]
    assert (
        len(subdirs) == 1
    ), f"Expected exactly one subdirectory in {build}, instead got {subdirs}"
    if subdirs:
        os.chdir(subdirs[0])

    # Run pio check command
    cp = subprocess.run(
        [
            "pio",
            "check",
            "--skip-packages",
            "--src-filters=+<lib/src/>",
            f"--severity={MINIMUM_REPORT_SEVERTIY}",
            f"--fail-on-defect={MINIMUM_FAIL_SEVERTIY}",
            "--flags",
            "--inline-suppr",
        ],
    )
    return cp.returncode


if __name__ == "__main__":
    sys.exit(main())
