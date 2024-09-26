from pathlib import Path

_HERE = Path(__file__).resolve().parent
PROJECT_ROOT = _HERE.parent.parent
BUILD = PROJECT_ROOT / ".build"
