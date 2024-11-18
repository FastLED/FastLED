
import os
from pathlib import Path

HERE = Path(__file__).parent
os.chdir(str(HERE   ))

_COMPILER_DIR = Path("/js/fastled/src/platforms/wasm/compiler")

def make_links() -> None:
    files = [
        "platformio.ini",
        "wasm_compiler_flags.py",
        #"index.html",
        #"index.css",
        #"index.js",
        "Arduino.h",
        #"filewatcher.py",
        "compile.py",
        "run.py",
        "server.py",
        "prewarm.sh"
    ]
    for file in files:
        link_dst = Path("/js") / file
        src = _COMPILER_DIR / file
        if str(src).endswith(".sh"):
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
    make_links()
    os.system("pio settings set check_platformio_interval 9999")
    os.system("pio settings set enable_telemetry 0")

if __name__ == "__main__":
    init_runtime()
