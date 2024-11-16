
import os
from pathlib import Path

_COMPILER_DIR = Path("/js/fastled/src/platforms/wasm/compiler")

def make_links() -> None:
    files = [
        "platformio.ini",
        "wasm_compiler_flags.py",
        "index.html",
        "index.css",
        "index.js",
        "Arduino.h",
        "filewatcher.py",
        "compile.py",
        "run.py",
        "server.py",
    ]
    for file in files:
        link_dst = Path("/js") / file
        if not link_dst.exists():
            print(f"Linking {_COMPILER_DIR / file} to /js/{file}")
            os.symlink(str(_COMPILER_DIR / file), str(link_dst))
        else:
            print(f"Target {link_dst} already exists")

def init_runtime() -> None:
    make_links()

if __name__ == "__main__":
    init_runtime()
