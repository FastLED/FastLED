import os
import subprocess
from pathlib import Path


def main():
    here = Path(__file__).parent
    project_root = here.parent.parent
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
    subprocess.run(
        [
            "pio",
            "check",
            "--skip-packages",
            "--src-filters=+<lib/src/>",
            "--severity=medium",
            "--fail-on-defect=high",
            "--flags",
            "--inline-suppr",
        ],
        check=True,
    )


if __name__ == "__main__":
    main()
