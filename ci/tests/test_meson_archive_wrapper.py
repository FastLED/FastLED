from pathlib import Path

from ci.meson.meson_markers import inject_ar_optimization_patches


def test_archive_wrapper_drops_meson_shell_cleanup(tmp_path: Path) -> None:
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    (build_dir / "build.ninja").write_text(
        "rule STATIC_LINKER\n"
        " command = rm -f $out && /usr/bin/llvm-ar $LINK_ARGS $out $in\n"
        " description = Linking static target $out\n\n",
        encoding="utf-8",
    )

    source_dir = Path(__file__).resolve().parents[2]
    assert inject_ar_optimization_patches(build_dir, source_dir)

    rule = (build_dir / "build.ninja").read_text(encoding="utf-8")
    assert 'ar_content_preserving.py" /usr/bin/llvm-ar' in rule
    assert "rm -f $out" not in rule
    assert "restat = 1" in rule


def test_archive_wrapper_repairs_existing_bad_rule(tmp_path: Path) -> None:
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    source_dir = Path(__file__).resolve().parents[2]
    wrapper = source_dir / "ci" / "meson" / "ar_content_preserving.py"
    (build_dir / "build.ninja").write_text(
        "rule STATIC_LINKER\n"
        f' command = "python" "{wrapper}" rm -f $out && /usr/bin/llvm-ar '
        "$LINK_ARGS $out $in\n"
        " description = Linking static target $out\n"
        " restat = 1\n\n",
        encoding="utf-8",
    )

    assert inject_ar_optimization_patches(build_dir, source_dir)
    rule = (build_dir / "build.ninja").read_text(encoding="utf-8")
    assert "rm -f $out" not in rule
    assert 'ar_content_preserving.py" /usr/bin/llvm-ar' in rule
