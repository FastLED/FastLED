import subprocess
from pathlib import Path


def map_dump(map_file: Path) -> None:
    map_arg = str(map_file)
    cmds: list[list[str]] = [
        ["uv", "run", "fpvgcc", map_arg, "--sar"],
        ["uv", "run", "fpvgcc", map_arg, "--lmap", "root"],
        ["uv", "run", "fpvgcc", map_arg, "--uf"],
        ["uv", "run", "fpvgcc", map_arg, "--uregions"],
        ["uv", "run", "fpvgcc", map_arg, "--usections"],
        ["uv", "run", "fpvgcc", map_arg, "--la"],
    ]
    for cmd in cmds:
        print("\nRunning command:", " ".join(cmd))
        subprocess.run(cmd, check=False)
