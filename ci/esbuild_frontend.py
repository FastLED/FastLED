from __future__ import annotations

import hashlib
import os
import shutil
import stat
import subprocess
import sys
import tarfile
from pathlib import Path

import httpx


ESBUILD_VERSION = "0.28.0"
PROJECT_ROOT = Path(__file__).parent.parent
FRONTEND_DIR = PROJECT_ROOT / "src" / "platforms" / "wasm" / "compiler"
INSTALL_ROOT = Path.home() / ".fastled" / "toolchains" / "esbuild"
ARCHIVE_CACHE_DIR = Path.home() / ".fastled" / "toolchains" / "archives"


def _platform_arch() -> tuple[str, str]:
    if sys_platform := sys.platform:
        if sys_platform == "win32":
            platform = "win32"
        elif sys_platform == "darwin":
            platform = "darwin"
        else:
            platform = "linux"
    else:
        platform = "linux"

    machine = os.uname().machine.lower() if hasattr(os, "uname") else "amd64"
    if machine in ("x86_64", "amd64"):
        arch = "x64"
    elif machine in ("arm64", "aarch64"):
        arch = "arm64"
    else:
        raise RuntimeError(f"Unsupported architecture for esbuild: {machine}")
    return platform, arch


def _tarball_url(platform: str, arch: str) -> str:
    package_tail = f"{platform}-{arch}"
    return f"https://registry.npmjs.org/@esbuild/{package_tail}/-/{package_tail}-{ESBUILD_VERSION}.tgz"


def install_esbuild(force: bool = False) -> Path:
    platform, arch = _platform_arch()
    install_dir = INSTALL_ROOT / platform / arch / ESBUILD_VERSION
    install_dir.mkdir(parents=True, exist_ok=True)
    exe_name = "esbuild.exe" if platform == "win32" else "esbuild"
    esbuild_path = install_dir / exe_name
    done_file = install_dir / "done.txt"
    if done_file.exists() and esbuild_path.exists() and not force:
        return esbuild_path

    ARCHIVE_CACHE_DIR.mkdir(parents=True, exist_ok=True)
    archive_path = (
        ARCHIVE_CACHE_DIR / f"esbuild-{platform}-{arch}-{ESBUILD_VERSION}.tgz"
    )
    if force and archive_path.exists():
        archive_path.unlink()

    if not archive_path.exists():
        response = httpx.get(
            _tarball_url(platform, arch), follow_redirects=True, timeout=120
        )
        response.raise_for_status()
        archive_path.write_bytes(response.content)

    with tarfile.open(archive_path, mode="r:gz") as tf:
        member_name = (
            f"package/{exe_name}" if platform == "win32" else "package/bin/esbuild"
        )
        member = tf.getmember(member_name)
        extracted = tf.extractfile(member)
        if extracted is None:
            raise RuntimeError(f"Could not extract {member_name} from {archive_path}")
        esbuild_path.write_bytes(extracted.read())

    if platform != "win32":
        current_mode = esbuild_path.stat().st_mode
        esbuild_path.chmod(current_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    done_file.write_text("ok\n", encoding="utf-8")
    return esbuild_path


def _copy_tree(src: Path, dst: Path) -> None:
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def _copy_file(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def _get_source_mtime(source_dir: Path) -> float:
    max_mtime = 0.0
    for path in source_dir.rglob("*"):
        if "dist" in path.parts:
            continue
        if path.is_file():
            max_mtime = max(max_mtime, path.stat().st_mtime)
    return max_mtime


def _compute_dir_hash(directory: Path) -> str:
    digest = hashlib.md5(usedforsecurity=False)
    for file_path in sorted(p for p in directory.rglob("*") if p.is_file()):
        digest.update(str(file_path.relative_to(directory)).encode("utf-8"))
        digest.update(file_path.read_bytes())
    return digest.hexdigest()


def _run_esbuild(args: list[str]) -> None:
    esbuild = install_esbuild()
    result = subprocess.run(
        [
            str(esbuild),
            "--alias:three=./vendor/three/build/three.module.js",
            *args,
        ],
        cwd=str(FRONTEND_DIR),
    )
    if result.returncode != 0:
        raise RuntimeError(f"esbuild failed with exit code {result.returncode}")


def build_dist() -> Path:
    dist_dir = FRONTEND_DIR / "dist"
    marker = dist_dir / ".esbuild_marker"
    source_mtime = _get_source_mtime(FRONTEND_DIR)
    if dist_dir.exists() and marker.exists():
        try:
            if float(marker.read_text(encoding="utf-8").strip()) >= source_mtime:
                return dist_dir
        except ValueError:
            pass

    if dist_dir.exists():
        shutil.rmtree(dist_dir)
    dist_dir.mkdir(parents=True, exist_ok=True)

    _run_esbuild(
        [
            str(FRONTEND_DIR / "app.ts"),
            "--bundle",
            "--format=esm",
            "--platform=browser",
            "--target=es2021",
            "--sourcemap",
            f"--outfile={FRONTEND_DIR / 'dist' / 'app.js'}",
            "--log-level=warning",
        ]
    )
    _run_esbuild(
        [
            str(FRONTEND_DIR / "modules" / "core" / "fastled_background_worker.ts"),
            "--bundle",
            "--format=esm",
            "--platform=browser",
            "--target=es2021",
            "--sourcemap",
            f"--outfile={FRONTEND_DIR / 'dist' / 'fastled_background_worker.js'}",
            "--log-level=warning",
        ]
    )

    index_html = (
        (FRONTEND_DIR / "index.html")
        .read_text(encoding="utf-8")
        .replace("./app.ts", "./app.js")
    )
    (dist_dir / "index.html").write_text(index_html, encoding="utf-8")
    _copy_file(FRONTEND_DIR / "index.css", dist_dir / "index.css")
    _copy_file(
        FRONTEND_DIR / "modules" / "audio" / "audio_worklet_processor.js",
        dist_dir / "audio_worklet_processor.js",
    )
    assets_dir = FRONTEND_DIR / "assets"
    if assets_dir.exists():
        _copy_tree(assets_dir, dist_dir / "assets")

    marker.write_text(str(source_mtime), encoding="utf-8")
    return dist_dir


def copy_dist_to_output(output_dir: Path) -> None:
    dist_dir = build_dist()
    hash_marker = output_dir / ".frontend_hash"
    current_hash = _compute_dir_hash(dist_dir)
    if (
        hash_marker.exists()
        and hash_marker.read_text(encoding="utf-8").strip() == current_hash
    ):
        print("[WASM] Frontend assets unchanged, skipping copy")
        return

    for item in dist_dir.iterdir():
        destination = output_dir / item.name
        if item.is_dir():
            _copy_tree(item, destination)
        else:
            _copy_file(item, destination)

    hash_marker.write_text(current_hash, encoding="utf-8")
