from __future__ import annotations

from pathlib import Path

from ci import wasm_build


def test_fast_link_invalidates_cache_when_js_glue_fingerprint_changes(
    tmp_path: Path, monkeypatch
) -> None:
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    sketch_object = tmp_path / "sketch.o"
    cached_wasm = tmp_path / "fastled.wasm"
    library_archive = tmp_path / "libfastled.a"

    sketch_object.write_bytes(b"obj")
    cached_wasm.write_bytes(b"wasm")
    library_archive.write_bytes(b"archive")
    (build_dir / "fastled_glue.js").write_text("glue", encoding="utf-8")
    (build_dir / "libemscripten_js_symbols.so").write_text("stub", encoding="utf-8")
    (build_dir / "wasm_ld_args.json").write_text(
        '["wasm-ld","{sketch_o}","-o","{output_wasm}"]', encoding="utf-8"
    )
    (build_dir / "wasm_ld_args.key").write_text(
        wasm_build._link_cache_key(library_archive), encoding="utf-8"
    )
    (build_dir / "js_glue_fingerprint").write_text("stale-js", encoding="utf-8")
    (build_dir / "link_environment_fingerprint").write_text(
        "current-env", encoding="utf-8"
    )

    called = False

    def fail_run(*args, **kwargs):
        nonlocal called
        called = True
        raise AssertionError("RunningProcess.run should not be reached on JS mismatch")

    monkeypatch.setattr(wasm_build.RunningProcess, "run", fail_run)
    monkeypatch.setattr(wasm_build, "_compute_js_glue_fingerprint", lambda: "fresh-js")
    monkeypatch.setattr(
        wasm_build, "_compute_link_environment_fingerprint", lambda mode: "current-env"
    )

    ok = wasm_build._fast_link(
        sketch_object, cached_wasm, build_dir, "quick", library_archive
    )

    assert ok is False
    assert called is False
    assert not (build_dir / "wasm_ld_args.json").exists()
    assert not (build_dir / "wasm_ld_args.key").exists()
    assert not (build_dir / "js_glue_fingerprint").exists()


def test_fast_link_invalidates_cache_when_link_environment_changes(
    tmp_path: Path, monkeypatch
) -> None:
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    sketch_object = tmp_path / "sketch.o"
    cached_wasm = tmp_path / "fastled.wasm"
    library_archive = tmp_path / "libfastled.a"

    sketch_object.write_bytes(b"obj")
    cached_wasm.write_bytes(b"wasm")
    library_archive.write_bytes(b"archive")
    (build_dir / "fastled_glue.js").write_text("glue", encoding="utf-8")
    (build_dir / "libemscripten_js_symbols.so").write_text("stub", encoding="utf-8")
    (build_dir / "wasm_ld_args.json").write_text(
        '["wasm-ld","{sketch_o}","-o","{output_wasm}"]', encoding="utf-8"
    )
    (build_dir / "wasm_ld_args.key").write_text(
        wasm_build._link_cache_key(library_archive), encoding="utf-8"
    )
    (build_dir / "js_glue_fingerprint").write_text("current-js", encoding="utf-8")
    (build_dir / "link_environment_fingerprint").write_text(
        "stale-env", encoding="utf-8"
    )

    called = False

    def fail_run(*args, **kwargs):
        nonlocal called
        called = True
        raise AssertionError("RunningProcess.run should not be reached on env mismatch")

    monkeypatch.setattr(wasm_build.RunningProcess, "run", fail_run)
    monkeypatch.setattr(
        wasm_build, "_compute_js_glue_fingerprint", lambda: "current-js"
    )
    monkeypatch.setattr(
        wasm_build, "_compute_link_environment_fingerprint", lambda mode: "fresh-env"
    )

    ok = wasm_build._fast_link(
        sketch_object, cached_wasm, build_dir, "quick", library_archive
    )

    assert ok is False
    assert called is False
    assert not (build_dir / "wasm_ld_args.json").exists()
    assert not (build_dir / "wasm_ld_args.key").exists()
    assert not (build_dir / "js_glue_fingerprint").exists()
    assert not (build_dir / "link_environment_fingerprint").exists()


def test_fast_link_uses_cache_when_fingerprints_match(
    tmp_path: Path, monkeypatch
) -> None:
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    sketch_object = tmp_path / "sketch.o"
    cached_wasm = tmp_path / "fastled.wasm"
    library_archive = tmp_path / "libfastled.a"

    sketch_object.write_bytes(b"obj")
    cached_wasm.write_bytes(b"wasm")
    library_archive.write_bytes(b"archive")
    (build_dir / "fastled_glue.js").write_text("glue", encoding="utf-8")
    (build_dir / "libemscripten_js_symbols.so").write_text("stub", encoding="utf-8")
    (build_dir / "wasm_ld_args.json").write_text(
        '["wasm-ld","{sketch_o}","-o","{output_wasm}"]', encoding="utf-8"
    )
    (build_dir / "wasm_ld_args.key").write_text(
        wasm_build._link_cache_key(library_archive), encoding="utf-8"
    )
    (build_dir / "js_glue_fingerprint").write_text("current-js", encoding="utf-8")
    (build_dir / "link_environment_fingerprint").write_text(
        "current-env", encoding="utf-8"
    )

    commands: list[list[str]] = []

    class Result:
        returncode = 0

    def fake_run(cmd, cwd=None):
        commands.append(cmd)
        return Result()

    monkeypatch.setattr(wasm_build.RunningProcess, "run", fake_run)
    monkeypatch.setattr(
        wasm_build, "_compute_js_glue_fingerprint", lambda: "current-js"
    )
    monkeypatch.setattr(
        wasm_build, "_compute_link_environment_fingerprint", lambda mode: "current-env"
    )

    ok = wasm_build._fast_link(
        sketch_object, cached_wasm, build_dir, "quick", library_archive
    )

    assert ok is True
    assert commands
    assert (cached_wasm.parent / "fastled.js").read_text(encoding="utf-8") == "glue"


def test_link_environment_fingerprint_changes_when_emcc_version_changes(
    monkeypatch,
) -> None:
    monkeypatch.setattr(wasm_build, "get_link_flags", lambda mode: ["-O1"])
    monkeypatch.setattr(wasm_build, "_cached_emcc_version_key", None)
    monkeypatch.setattr(wasm_build, "_cached_emcc_version_value", None)
    monkeypatch.setattr(wasm_build, "_cached_emcc_version_time", 0.0)

    versions = iter(["emcc 1.0.0", "emcc 2.0.0"])

    class Result:
        def __init__(self, text: str) -> None:
            self.stdout = text
            self.stderr = ""

    def fake_run(cmd, **kwargs):
        return Result(next(versions))

    monkeypatch.setattr(wasm_build.RunningProcess, "run", fake_run)

    first = wasm_build._compute_link_environment_fingerprint("quick")
    monkeypatch.setattr(wasm_build, "_cached_emcc_version_time", 0.0)
    second = wasm_build._compute_link_environment_fingerprint("quick")

    assert first != second
