import json
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Tools:
    as_path: Path
    ld_path: Path
    objcopy_path: Path
    objdump_path: Path
    cpp_filt_path: Path
    nm_path: Path


def load_tools(build_info_path: Path) -> Tools:
    build_info = json.loads(build_info_path.read_text())
    board_info = build_info[next(iter(build_info))]
    aliases = board_info["aliases"]
    as_path = Path(aliases["as"])
    ld_path = Path(aliases["ld"])
    objcopy_path = Path(aliases["objcopy"])
    objdump_path = Path(aliases["objdump"])
    cpp_filt_path = Path(aliases["c++filt"])
    nm_path = Path(aliases["nm"])
    if sys.platform == "win32":
        as_path = as_path.with_suffix(".exe")
        ld_path = ld_path.with_suffix(".exe")
        objcopy_path = objcopy_path.with_suffix(".exe")
        objdump_path = objdump_path.with_suffix(".exe")
        cpp_filt_path = cpp_filt_path.with_suffix(".exe")
        nm_path = nm_path.with_suffix(".exe")
    out = Tools(as_path, ld_path, objcopy_path, objdump_path, cpp_filt_path, nm_path)
    tools = [as_path, ld_path, objcopy_path, objdump_path, cpp_filt_path, nm_path]
    for tool in tools:
        if not tool.exists():
            raise FileNotFoundError(f"Tool not found: {tool}")
    return out
