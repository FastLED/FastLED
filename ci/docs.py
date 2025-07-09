"""
Work in progress to generate doxygen via a script instead of a GitHub action.
"""

import os
import platform
import shutil
import subprocess
import warnings
from pathlib import Path
from typing import Optional, Tuple

from download import download  # type: ignore


# Configs
DOXYGEN_VERSION = (
    "1.13.2"  # DOXYGEN_AWESOME styler is has certain restrictions with doxygen version
)
DOXYGEN_AWESOME_VERSION = "2.3.4"  # deprecating
DOXYFILE_PATH = Path("docs/Doxyfile")
HTML_OUTPUT_DIR = Path("docs/html")
DOXYGEN_CSS_REPO = "https://github.com/jothepro/doxygen-awesome-css"  # deprecating


HERE = Path(__file__).parent.resolve()
PROJECT_ROOT = HERE.parent

DOCS_ROOT = PROJECT_ROOT / "docs"
DOCS_TOOL_PATH = PROJECT_ROOT / ".tools_cache"
DOCS_OUTPUT_PATH = DOCS_ROOT / "html"


def run(
    cmd: str,
    cwd: Optional[str] = None,
    shell: bool = True,
    check: bool = True,
    capture: bool = True,
) -> str:
    print(f"Running: {cmd}")
    result = subprocess.run(
        cmd, shell=shell, cwd=cwd, check=False, capture_output=capture, text=False
    )
    if capture:
        stdout = result.stdout.decode("utf-8") if result.stdout else ""
        stderr = result.stderr.decode("utf-8") if result.stderr else ""
    else:
        stdout = ""
        stderr = ""
    if result.returncode != 0:
        msg = f"Command failed with exit code {result.returncode}:\nstdout:\n{stdout}\n\nstderr:\n{stderr}"
        warnings.warn(msg)
        if check:
            raise subprocess.CalledProcessError(
                result.returncode, cmd, output=result.stdout
            )
    return stdout.strip()


def get_git_info() -> Tuple[str, str]:
    release_tag = os.environ.get("RELEASE_TAG", "")

    try:
        latest_tag = run("git tag | grep -E '^[0-9]' | sort -V | tail -1")
        latest_tag = latest_tag if latest_tag else ""
    except subprocess.CalledProcessError:
        latest_tag = ""

    git_sha_short = run("git rev-parse --short HEAD")
    full_sha = run("git rev-parse HEAD")
    project_number = release_tag or latest_tag or git_sha_short
    commit_message = (
        f"{project_number} ({full_sha})"
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
    zip_path = DOCS_TOOL_PATH / f"doxygen-{DOXYGEN_VERSION}.zip"
    extract_dir = DOCS_TOOL_PATH / f"doxygen-{DOXYGEN_VERSION}"

    # Create tool path if it doesn't exist
    DOCS_TOOL_PATH.mkdir(exist_ok=True, parents=True)

    download(doxygen_url, zip_path)
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

    # Create tool path if it doesn't exist
    DOCS_TOOL_PATH.mkdir(exist_ok=True, parents=True)

    # Change to tool directory for download and extraction
    original_dir = os.getcwd()
    os.chdir(str(DOCS_TOOL_PATH))

    try:
        run(f"wget -q {url}")
        run(f"tar -xf {archive}")
        bin_dir = DOCS_TOOL_PATH / f"doxygen-{DOXYGEN_VERSION}"
        return bin_dir / "bin" / "doxygen"
    finally:
        os.chdir(original_dir)


def install_theme() -> Path:
    print("Installing Doxygen Awesome Theme...")
    theme_path = DOCS_ROOT / "doxygen-awesome-css"
    if theme_path.exists():
        return theme_path
    run(
        f"git clone --depth 1 -b v{DOXYGEN_AWESOME_VERSION} {DOXYGEN_CSS_REPO}",
        cwd=str(DOCS_ROOT),
    )
    return theme_path


def generate_docs(doxygen_bin: Path) -> None:
    print("Generating documentation...")
    cmd_str = f'"{doxygen_bin}" {DOXYFILE_PATH.name}'
    run(cmd_str, cwd=str(DOCS_ROOT), capture=False)


# def install_graphviz() -> None:
#     url: str = get_latest_release_for_platform()
#     print(url)


def main() -> None:
    is_windows = platform.system() == "Windows"
    # is_macos = platform.system() == "Darwin"
    _, commit_msg = get_git_info()

    if is_windows:
        doxygen_bin = install_doxygen_windows()
        # add to path C:\Program Files\Graphviz\bin\
        os.environ["PATH"] += os.pathsep + r"C:\Program Files\Graphviz\bin"
    else:
        doxygen_bin = install_doxygen_unix()

    # install_theme()

    # install_graphviz()  # Work in progress

    # Verify Graphviz installation
    try:
        dot_version = run("dot -V", check=False)
        print(f"Graphviz detected: {dot_version}")
    except Exception:
        warnings.warn(
            "Graphviz (dot) not found in PATH. Diagrams may not be generated."
        )

    # Check it graphviz is installed
    # if linux
    if not is_windows:
        run("dot -Tsvg -Kneato -Grankdir=LR", check=True)

    generate_docs(doxygen_bin=doxygen_bin)

    print(f"\n✅ Docs generated in: {HTML_OUTPUT_DIR}")
    print(f"📄 Commit message: {commit_msg}")
    print("✨ You can now manually deploy to GitHub Pages or automate this step.")


if __name__ == "__main__":
    main()
