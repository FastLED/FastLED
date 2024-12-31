
import os
from pathlib import Path
import glob
import warnings
from concurrent.futures import ThreadPoolExecutor

HERE = Path(__file__).parent


_COMPILER_DIR = Path("/js/fastled/src/platforms/wasm/compiler")


def task(src: str | Path) -> None:
    src = Path(src)
    if "entrypoint.sh" in str(src):
        return
    link_dst = Path("/js") / src.name
    
    # Handle shell scripts
    if src.suffix == '.sh':
        os.system(f"dos2unix {src} && chmod +x {src}")

    # if link exists, remove it
    if link_dst.exists():
        print(f"Removing existing link {link_dst}")
        try:
            os.remove(link_dst)
        except Exception as e:
            warnings.warn(f"Failed to remove {link_dst}: {e}")

        
    if not link_dst.exists():
        print(f"Linking {src} to {link_dst}")
        try:
            os.symlink(str(src), str(link_dst))
        except FileExistsError:
            print(f"Target {link_dst} already exists")
    else:
        print(f"Target {link_dst} already exists")


def make_links() -> None:
    # Define file patterns to include
    patterns = ['*.h', '*.py', '*.css', '*.sh', "*.ino", "*.hpp", "*.cpp", "*.ini", "*.txt"]
    
    # Get all matching files in compiler directory
    files = []
    for pattern in patterns:
        files.extend(glob.glob(str(_COMPILER_DIR / pattern)))

    # Process files in parallel using ThreadPoolExecutor
    with ThreadPoolExecutor(max_workers=16) as executor:
        executor.map(task, files)


def init_runtime() -> None:
    os.chdir(str(HERE))
    make_links()


if __name__ == "__main__":
    init_runtime()
