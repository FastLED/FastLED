"""
Work in progress to generate doxygen via a script instead of a GitHub action.
"""

import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import Optional, Tuple
from urllib.request import urlretrieve

# Configs
DOXYGEN_VERSION = "1.11.0"
DOXYGEN_AWESOME_VERSION = "2.3.3"
DOXYFILE_PATH = Path("docs/Doxyfile")
HTML_OUTPUT_DIR = Path("docs/html")
DOXYGEN_CSS_REPO = "https://github.com/jothepro/doxygen-awesome-css"


def run(
    cmd: str, cwd: Optional[str] = None, shell: bool = True, check: bool = True
) -> str:
    print(f"Running: {cmd}")
    result = subprocess.run(
        cmd, shell=shell, cwd=cwd, check=check, capture_output=True, text=True
    )
    return result.stdout.strip()


def get_git_info() -> Tuple[str, str]:
    run("git fetch --prune --unshallow --tags")
    try:
        release_tag = os.environ.get("RELEASE_TAG", "")
        latest_tag = run("git tag | grep -E '^[0-9]' | sort -V | tail -1")
    except subprocess.CalledProcessError:
        raise RuntimeError("No tags found in the repository.")

    git_sha_short = run("git rev-parse --short HEAD")
    project_number = release_tag or latest_tag or git_sha_short
    commit_message = (
        f"{project_number} ({run('git rev-parse HEAD')})"
        if project_number != git_sha_short
        else project_number
    )
    print(f"Project number: {project_number}")
    print(f"Commit message: {commit_message}")
    return project_number, commit_message


def install_doxygen_windows() -> Path:
    print("Installing Doxygen...")
    doxygen_url = (
        f"https://www.doxygen.nl/files/doxygen-{DOXYGEN_VERSION}.windows.x64.bin.zip"
    )
    zip_path = Path(tempfile.gettempdir()) / "doxygen.zip"
    extract_dir = Path(tempfile.gettempdir()) / f"doxygen-{DOXYGEN_VERSION}"

    urlretrieve(doxygen_url, zip_path)
    shutil.unpack_archive(str(zip_path), extract_dir)
    bin_path = next(extract_dir.glob("**/doxygen.exe"), None)
    if not bin_path:
        raise FileNotFoundError("Doxygen executable not found after extraction.")
    print(f"Doxygen installed at: {bin_path}")
    return bin_path


def install_doxygen_unix() -> Path:
    print("Installing Doxygen...")
    archive = f"doxygen-{DOXYGEN_VERSION}.linux.bin.tar.gz"
    url = f"https://www.doxygen.nl/files/{archive}"
    run(f"wget -q {url}")
    run(f"tar -xf {archive}")
    bin_dir = Path(f"doxygen-{DOXYGEN_VERSION}")
    return bin_dir / "bin" / "doxygen"


def install_theme() -> Path:
    print("Installing Doxygen Awesome Theme...")
    theme_path = Path("docs/doxygen-awesome-css")
    if theme_path.exists():
        shutil.rmtree(theme_path)
    run(
        f"git clone --depth 1 -b v{DOXYGEN_AWESOME_VERSION} {DOXYGEN_CSS_REPO}",
        cwd="docs",
    )
    return theme_path


def update_doxyfile(project_number: str) -> None:
    print("Updating Doxyfile with project number...")
    doxyfile = DOXYFILE_PATH.read_text()
    updated = re.sub(
        r"(?m)^PROJECT_NUMBER\s*=.*", f"PROJECT_NUMBER = {project_number}", doxyfile
    )
    DOXYFILE_PATH.write_text(updated)


def generate_docs(doxygen_bin: Path) -> None:
    print("Generating documentation...")
    run(f'"{doxygen_bin}" Doxyfile', cwd="docs")


def main() -> None:
    is_windows = os.name == "nt"
    project_number, commit_msg = get_git_info()

    if is_windows:
        doxygen_bin = install_doxygen_windows()
    else:
        doxygen_bin = install_doxygen_unix()

    install_theme()
    update_doxyfile(project_number)
    generate_docs(doxygen_bin)

    print(f"\n✅ Docs generated in: {HTML_OUTPUT_DIR.resolve()}")
    print(f"📄 Commit message: {commit_msg}")
    print("✨ You can now manually deploy to GitHub Pages or automate this step.")


if __name__ == "__main__":
    main()
