from pathlib import Path

from ci.lint.license_headers import verify_manifest, verify_path_coverage


def test_license_artifact_manifest_detects_changes(tmp_path: Path) -> None:
    artifact = tmp_path / "artifact.txt"
    artifact.write_bytes(b"canonical\n")
    manifest = tmp_path / "manifest.sha256"
    manifest.write_text(
        "43045e07e709b38e470076ff8235b68ca6e63400498c0aa847f6e743f230166e"
        "  artifact.txt\n",
        encoding="utf-8",
    )
    assert verify_manifest(tmp_path, manifest.name)
    artifact.write_bytes(b"changed\n")
    assert not verify_manifest(tmp_path, manifest.name)


def test_license_artifact_manifest_rejects_path_escape(tmp_path: Path) -> None:
    manifest = tmp_path / "manifest.sha256"
    manifest.write_text(f"{'0' * 64}  ../outside.txt\n", encoding="utf-8")
    assert not verify_manifest(tmp_path, manifest.name)


def test_excluded_manifest_requires_exact_inventory_coverage() -> None:
    inventory = {"vendor/a.h", "vendor/b.cpp"}
    assert verify_path_coverage(inventory, inventory)
    assert not verify_path_coverage({"vendor/a.h"}, inventory)
    assert not verify_path_coverage(inventory | {"vendor/stale.c"}, inventory)
