"""Install the pinned reld release for the current CI host."""

import hashlib
import os
import platform
import shutil
import stat
import tarfile
import tempfile
import zipfile
from pathlib import Path, PurePosixPath
from urllib.request import urlopen


VERSION = "v0.2.0"
SHA256 = {
    "aarch64-apple-darwin": "8e0ff6abefd1cdab2da753d135cf30a5eca2a14b02624fe5ae5a34053cc8b32c",
    "aarch64-pc-windows-msvc": "f00a244f044a663512b2523cb12223eefd44c2500360fe8c1eb283e6eeac605d",
    "aarch64-unknown-linux-gnu": "b04a8f50267fce8e0c4e1090fccee1da4184ca5e2da9bc10d163fd86f6d11d04",
    "x86_64-apple-darwin": "6614030adb92590097d551a721f8f9b04b1a4df0ad3ba0f0e99d3fa90428b3bf",
    "x86_64-pc-windows-msvc": "1c848395af3cca223d4a14bbd4da6b8bfd1e0ed618b255ca4b6839bf997bcc88",
    "x86_64-unknown-linux-gnu": "ee2546bc08063bde20f7692cb064cccb0438ae83eaf663b1e934f2fcbdf9e974",
}


def host_target(system: str, machine: str) -> str:
    os_suffix = {
        "Linux": "unknown-linux-gnu",
        "Darwin": "apple-darwin",
        "Windows": "pc-windows-msvc",
    }.get(system)
    arch = {
        "AMD64": "x86_64",
        "ARM64": "aarch64",
        "x86_64": "x86_64",
        "arm64": "aarch64",
        "aarch64": "aarch64",
    }.get(machine)
    target = f"{arch}-{os_suffix}"
    if target not in SHA256:
        raise RuntimeError(f"Unsupported reld host: {system} {machine}")
    return target


def safe_member(name: str, root: str) -> Path:
    path = PurePosixPath(name)
    if (
        not name
        or "\\" in name
        or path.is_absolute()
        or ".." in path.parts
        or path.parts[0] != root
        or ":" in name
    ):
        raise ValueError(f"Unsafe reld archive path: {name}")
    return Path(*path.parts)


def extract(archive: Path, destination: Path, root: str) -> None:
    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as source:
            for member in source.infolist():
                relative = safe_member(member.filename, root)
                mode = member.external_attr >> 16
                if stat.S_ISLNK(mode):
                    raise ValueError(f"Archive link is forbidden: {member.filename}")
                output = destination / relative
                if member.is_dir():
                    output.mkdir(parents=True, exist_ok=True)
                else:
                    output.parent.mkdir(parents=True, exist_ok=True)
                    with (
                        source.open(member) as input_file,
                        output.open("wb") as output_file,
                    ):
                        shutil.copyfileobj(input_file, output_file)
    else:
        with tarfile.open(archive, "r:gz") as source:
            for member in source:
                relative = safe_member(member.name, root)
                if not (member.isdir() or member.isfile()):
                    raise ValueError(
                        f"Archive special file is forbidden: {member.name}"
                    )
                output = destination / relative
                if member.isdir():
                    output.mkdir(parents=True, exist_ok=True)
                else:
                    output.parent.mkdir(parents=True, exist_ok=True)
                    input_file = source.extractfile(member)
                    if input_file is None:
                        raise ValueError(f"Cannot read archive file: {member.name}")
                    with (
                        input_file,
                        output.open("wb") as output_file,
                    ):
                        shutil.copyfileobj(input_file, output_file)
                    output.chmod(member.mode & 0o777)


def main() -> None:
    runner_temp = Path(os.environ["RUNNER_TEMP"]).resolve()
    github_env = Path(os.environ["GITHUB_ENV"])
    target = host_target(platform.system(), platform.machine())
    root = f"reld-{VERSION}-{target}"
    archive_name = root + (".zip" if platform.system() == "Windows" else ".tar.gz")
    install_dir = Path(
        tempfile.mkdtemp(prefix=f"fastled-reld-{VERSION}-", dir=runner_temp)
    )
    archive = install_dir / archive_name
    url = f"https://github.com/zackees/reld/releases/download/{VERSION}/{archive_name}"
    with urlopen(url, timeout=120) as response, archive.open("wb") as output:
        shutil.copyfileobj(response, output)
    with archive.open("rb") as input_file:
        hasher = hashlib.sha256()
        for chunk in iter(lambda: input_file.read(1024 * 1024), b""):
            hasher.update(chunk)
        digest = hasher.hexdigest()
    if digest != SHA256[target]:
        raise ValueError(f"SHA-256 mismatch for {archive_name}: {digest}")
    extract(archive, install_dir, root)
    linker = (
        install_dir / root / ("reld.exe" if platform.system() == "Windows" else "reld")
    ).resolve()
    if not linker.is_file():
        raise FileNotFoundError(f"reld executable missing from {archive_name}")
    with github_env.open("a", encoding="utf-8") as output:
        output.write(f"FASTLED_NATIVE_LINKER={linker}\n")
    print(f"Installed {linker}")


if __name__ == "__main__":
    main()
