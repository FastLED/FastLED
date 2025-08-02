import os
from pathlib import Path


def map_dump(map_file: Path) -> None:
    # os.system("uv run fpvgcc ci/tests/uno/firmware.map --lmap root")

    cmds = [
        f"uv run fpvgcc {map_file} --sar",
        f"uv run fpvgcc {map_file} --lmap root",
        f"uv run fpvgcc {map_file} --uf",
        f"uv run fpvgcc {map_file} --uregions",
        # --usections
        f"uv run fpvgcc {map_file} --usections",
        f"uv run fpvgcc {map_file} --la",
    ]
    for cmd in cmds:
        print("\nRunning command: ", cmd)
        os.system(cmd)
