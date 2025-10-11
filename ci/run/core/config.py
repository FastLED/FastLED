from __future__ import annotations

import json
import os
from dataclasses import asdict, dataclass
from pathlib import Path


CONFIG_DIR = Path(os.path.expanduser("~/.fastledci"))
CONFIG_PATH = CONFIG_DIR / "config.json"


@dataclass
class Config:
    default_platform: str = "esp32s3"
    parallel_jobs: int = 4
    docker_default: bool = False

    @classmethod
    def load(cls) -> Config:
        try:
            data = json.loads(CONFIG_PATH.read_text())
            return cls(**data)
        except Exception:
            return cls()

    def save(self) -> None:
        CONFIG_DIR.mkdir(parents=True, exist_ok=True)
        CONFIG_PATH.write_text(json.dumps(asdict(self), indent=2))
