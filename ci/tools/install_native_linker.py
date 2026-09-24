"""Install the pinned upstream Wild release for Linux native CI links."""

import hashlib
import os
import platform
import shutil
import tarfile
import tempfile
from pathlib import Path, PurePosixPath
from urllib.request import urlopen


VERSION = "0.10.0"
SHA256 = {
    "aarch64-unknown-linux-gnu": "e9d670e41f76481a68984f816e25bd2f124664db3ac935053e1a6fc41d2894c2",
    "x86_64-unknown-linux-gnu": "641265506a7c06cfb03181b8916ab663ec8407855db6d4db7f8450667d105283",
}


def host_target(system: str, machine: str) -> str:
    if system != "Linux":
        raise RuntimeError(f"Wild native CI linker is Linux-only: {system}")
    arch = {
        "x86_64": "x86_64",
        "arm64": "aarch64",
        "aarch64": "aarch64",
    }.get(machine)
    target = f"{arch}-unknown-linux-gnu"
    if target not in SHA256:
        raise RuntimeError(f"Unsupported Wild host: {system} {machine}")
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
        raise ValueError(f"Unsafe Wild archive path: {name}")
    return Path(*path.parts)


def extract(archive: Path, destination: Path, root: str) -> None:
    with tarfile.open(archive, "r:gz") as source:
        for member in source:
            relative = safe_member(member.name, root)
            if not (member.isdir() or member.isfile()):
                raise ValueError(f"Archive special file is forbidden: {member.name}")
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
    root = f"wild-linker-{VERSION}-{target}"
    archive_name = root + ".tar.gz"
    install_dir = Path(
        tempfile.mkdtemp(prefix=f"fastled-wild-{VERSION}-", dir=runner_temp)
    )
    archive = install_dir / archive_name
    url = f"https://github.com/wild-linker/wild/releases/download/{VERSION}/{archive_name}"
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
    linker = (install_dir / root / "wild").resolve()
    if not linker.is_file():
        raise FileNotFoundError(f"Wild executable missing from {archive_name}")
    with github_env.open("a", encoding="utf-8") as output:
        output.write(f"FASTLED_NATIVE_LINKER={linker}\n")
    print(f"Installed {linker}")


if __name__ == "__main__":
    main()
