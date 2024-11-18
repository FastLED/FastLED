
import os
from pathlib import Path
import glob

HERE = Path(__file__).parent


_COMPILER_DIR = Path("/js/fastled/src/platforms/wasm/compiler")

def make_links() -> None:
    # Define file patterns to include
    patterns = ['*.h', '*.py', '*.css', '*.js', '*.sh', "*.ino", "*.hpp", "*.cpp", "*.ini"]
    
    # Get all matching files in compiler directory
    files = []
    for pattern in patterns:
        files.extend(glob.glob(str(_COMPILER_DIR / pattern)))
    
    for src in files:
        src = Path(src)
        link_dst = Path("/js") / src.name
        
        # Handle shell scripts
        if src.suffix == '.sh':
            os.system(f"dos2unix {src} && chmod +x {src}")
            
        if not link_dst.exists():
            print(f"Linking {src} to {link_dst}")
            try:
                os.symlink(str(src), str(link_dst))
            except FileExistsError:
                print(f"Target {link_dst} already exists")
        else:
            print(f"Target {link_dst} already exists")

def init_runtime() -> None:
    os.chdir(str(HERE))
    make_links()


if __name__ == "__main__":
    init_runtime()
