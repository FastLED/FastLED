Import("env")

# Does nothing right now. I was trying to make
# FastLED use a symlink to the code folder so
# that changes to the code would be reflected
# in the repo.

from pathlib import Path
import warnings

# PIO_DIR = Path(env.PioPlatform())

PROJECT_ROOT = Path(str(env.subst("$PROJECT_DIR"))).resolve()
LIBDEPS = PROJECT_ROOT / ".pio" / "libdeps"

def find_newest_libdeps() -> Path:
    target = LIBDEPS / env.subst("$PIOENV")
    if not target.exists():
        raise FileNotFoundError(f"Target libdeps not found: {target}")
    return target

try:
    print(str(find_newest_libdeps()))
except Exception as e:
    warnings.warn(str(e))
