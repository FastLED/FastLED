"""Content and dependency contracts for the per-profile AST cache."""

import hashlib
import os
from pathlib import Path
from typing import Callable, TypedDict

import pytest
from running_process import RunningProcess

from ci.lint_cpp.ast_cache import AstInputSnapshot, AstUnitResult, cached_ast_unit


class CachePaths(TypedDict):
    input_files: list[Path]
    include_roots: list[Path]
    cache_dir: Path


class CacheInputs(CachePaths):
    identity: dict[str, object]


class CacheCall(CacheInputs):
    runner: Callable[[], AstUnitResult]


class ProfileCall(CachePaths):
    runner: Callable[[], AstUnitResult]


def test_inventory_resolves_roots_not_every_ordinary_sdk_entry(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    root = tmp_path / "sdk"
    for index in range(30):
        header = root / "include" / f"component{index}" / "detail" / "header"
        header.parent.mkdir(parents=True)
        header.write_text("header", encoding="utf-8")
    original = Path.resolve
    resolved: list[Path] = []

    def resolve(path: Path, strict: bool = False) -> Path:
        resolved.append(path)
        return original(path, strict=strict)

    monkeypatch.setattr(Path, "resolve", resolve)
    snapshot = AstInputSnapshot()
    snapshot.inventory(root)
    assert len(snapshot._available_files[root]) == 30
    assert len(resolved) <= 2


def test_inventory_preserves_reference_digest_and_external_symlink_cycles(
    tmp_path: Path,
) -> None:
    root = tmp_path / "sdk"
    root.mkdir()
    outside = tmp_path / "external"
    outside.mkdir()
    (outside / "extensionless").write_text("outside", encoding="utf-8")
    (root / "SDKSettings.json").write_text("metadata", encoding="utf-8")
    (root / "external").symlink_to(outside, target_is_directory=True)
    (outside / "cycle").symlink_to(root, target_is_directory=True)
    (root / "missing").symlink_to(tmp_path / "absent")
    digest = hashlib.sha256()
    available: set[Path] = set()

    def reference(path: Path, ancestors: frozenset[Path]) -> None:
        resolved = path.resolve()
        digest.update(str(path).encode() + b"\0" + str(resolved).encode() + b"\0")
        if not path.exists():
            digest.update(b"missing\0")
        elif path.is_dir():
            digest.update(b"dir\0")
            if resolved not in ancestors:
                for child in sorted(path.iterdir()):
                    reference(child, ancestors | {resolved})
        else:
            digest.update(b"file\0")
            if path.is_file():
                available.add(resolved)

    reference(root, frozenset())
    snapshot = AstInputSnapshot()
    assert snapshot.inventory(root) == digest.hexdigest()
    assert snapshot._available_files[root] == available


def test_prepare_parse_hashes_sdk_candidate_set_once_per_snapshot(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    root = tmp_path / "sdk"
    root.mkdir()
    for index in range(20):
        (root / f"header{index}").write_text("header", encoding="utf-8")
    calls: list[Path] = []
    original = AstInputSnapshot.content

    def content(snapshot: AstInputSnapshot, path: Path) -> str:
        calls.append(path)
        return original(snapshot, path)

    monkeypatch.setattr(AstInputSnapshot, "content", content)
    snapshot = AstInputSnapshot()
    snapshot.prepare_parse([root], [])
    snapshot.prepare_parse([root], [])
    assert len(calls) == 20


@pytest.mark.skipif(os.name != "nt", reason="requires real Windows directory junctions")
def test_inventory_resolves_windows_junction_and_detects_external_content(
    tmp_path: Path,
) -> None:
    root = tmp_path / "sdk"
    outside = tmp_path / "external"
    root.mkdir()
    outside.mkdir()
    header = outside / "header.h"
    header.write_text("old", encoding="utf-8")
    junction = root / "headers"
    for link, target in ((junction, outside), (outside / "cycle", root)):
        result = RunningProcess.run(
            ["cmd", "/d", "/c", "mklink", "/J", str(link), str(target)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=30,
        )
        assert result.returncode == 0, result.stdout
    snapshot = AstInputSnapshot()
    snapshot.prepare_parse([root], [])
    assert header.resolve() in snapshot._available_files[root]
    snapshot.verify_parse([root], [header])
    stat = header.stat()
    header.write_text("new", encoding="utf-8")
    os.utime(header, ns=(stat.st_atime_ns, stat.st_mtime_ns))
    with pytest.raises(ValueError, match="changed during analysis"):
        snapshot.verify_parse([root], [header])


def test_content_hash_supports_python_310(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    # Python 3.10, the project's minimum version, has no file_digest helper.
    monkeypatch.delattr(hashlib, "file_digest", raising=False)
    path = tmp_path / "input"
    data = b"hash in bounded chunks" * 60000
    path.write_bytes(data)
    assert AstInputSnapshot().content(path) == hashlib.sha256(data).hexdigest()


def test_warm_replays_violations_without_parsing(tmp_path: Path) -> None:
    source = tmp_path / "unit.cpp"
    source.write_text("int value;", encoding="utf-8")
    calls = []

    def parse() -> AstUnitResult:
        calls.append(True)
        return AstUnitResult({"violations": ["missing noexcept"]}, (source,))

    kwargs: CacheCall = dict(
        identity={"profile": "esp"},
        input_files=[source],
        include_roots=[tmp_path / "include"],
        runner=parse,
        cache_dir=tmp_path / "cache",
    )
    cold = cached_ast_unit("warm", **kwargs)
    warm = cached_ast_unit("warm", **kwargs)
    assert not cold.cache_hit
    assert warm.cache_hit
    assert warm.payload == {"violations": ["missing noexcept"]}
    assert len(calls) == 1


@pytest.mark.parametrize(
    "changed", ["source", "header", "compiler", "matcher", "config"]
)
def test_content_edits_with_preserved_stat_invalidate(
    tmp_path: Path, changed: str
) -> None:
    files = {
        name: tmp_path / name
        for name in ["source", "header", "compiler", "matcher", "config"]
    }
    includes = tmp_path / "include"
    includes.mkdir()
    files["header"] = includes / "header"
    for path in files.values():
        path.write_text("old", encoding="utf-8")
    calls = []

    def parse() -> AstUnitResult:
        calls.append(True)
        return AstUnitResult({"ok": True}, (files["header"],))

    kwargs: CacheCall = dict(
        identity={"sdk": "idf4"},
        input_files=[path for name, path in files.items() if name != "header"],
        include_roots=[includes],
        runner=parse,
        cache_dir=tmp_path / "cache",
    )
    cached_ast_unit("content", **kwargs)
    stat = files[changed].stat()
    files[changed].write_text("new", encoding="utf-8")
    os.utime(files[changed], ns=(stat.st_atime_ns, stat.st_mtime_ns))
    assert not cached_ast_unit("content", **kwargs).cache_hit
    assert len(calls) == 2


@pytest.mark.parametrize("key", ["arguments", "sdk", "target", "matcher_version"])
def test_profile_identity_invalidates(tmp_path: Path, key: str) -> None:
    source = tmp_path / "source"
    source.write_text("source", encoding="utf-8")
    kwargs: ProfileCall = dict(
        input_files=[source],
        include_roots=[],
        runner=lambda: AstUnitResult({"ok": True}, (source,)),
        cache_dir=tmp_path / "cache",
    )
    cached_ast_unit("identity", identity={key: "old"}, **kwargs)
    assert not cached_ast_unit("identity", identity={key: "new"}, **kwargs).cache_hit


def test_negative_lookup_and_symlinked_header_availability(tmp_path: Path) -> None:
    includes = tmp_path / "include"
    external = tmp_path / "external"
    includes.mkdir()
    external.mkdir()
    (includes / "linked").symlink_to(external, target_is_directory=True)
    source = tmp_path / "unit.cpp"
    source.write_text("source", encoding="utf-8")
    kwargs: CacheCall = dict(
        identity={},
        input_files=[source],
        include_roots=[includes],
        runner=lambda: AstUnitResult({"ok": True}, (source,)),
        cache_dir=tmp_path / "cache",
    )
    cached_ast_unit("negative", **kwargs)
    assert cached_ast_unit("negative", **kwargs).cache_hit
    (external / "optional_without_extension").write_text("new", encoding="utf-8")
    assert not cached_ast_unit("negative", **kwargs).cache_hit
    assert cached_ast_unit("negative", **kwargs).cache_hit


def test_only_affected_translation_unit_reparses(tmp_path: Path) -> None:
    includes = tmp_path / "include"
    includes.mkdir()
    a = includes / "a.h"
    b = includes / "b.h"
    a.write_text("a", encoding="utf-8")
    b.write_text("b", encoding="utf-8")

    def run(name: str, dependency: Path):
        return cached_ast_unit(
            name,
            identity={},
            input_files=[],
            include_roots=[includes],
            runner=lambda: AstUnitResult({"ok": True}, (dependency,)),
            cache_dir=tmp_path / "cache",
        )

    run("a", a)
    run("b", b)
    a.write_text("changed", encoding="utf-8")
    assert not run("a", a).cache_hit
    assert run("b", b).cache_hit


def test_incomplete_and_raised_parses_are_never_cached(tmp_path: Path) -> None:
    kwargs: CacheInputs = dict(
        identity={}, input_files=[], include_roots=[], cache_dir=tmp_path / "cache"
    )

    def failed() -> AstUnitResult:
        raise RuntimeError("compiler error despite exit zero")

    for _ in range(2):
        with pytest.raises(RuntimeError, match="compiler error"):
            cached_ast_unit("failed", runner=failed, **kwargs)
        with pytest.raises(ValueError, match="incomplete"):
            cached_ast_unit(
                "incomplete",
                runner=lambda: AstUnitResult({}, (), complete=False),
                **kwargs,
            )
    assert not cached_ast_unit(
        "failed", runner=lambda: AstUnitResult({}, ()), **kwargs
    ).cache_hit


def test_deleted_dependency_is_not_a_hit(tmp_path: Path) -> None:
    header = tmp_path / "header"
    header.write_text("header", encoding="utf-8")
    kwargs: CacheInputs = dict(
        identity={},
        input_files=[header],
        include_roots=[],
        cache_dir=tmp_path / "cache",
    )
    cached_ast_unit("deleted", runner=lambda: AstUnitResult({}, (header,)), **kwargs)
    header.rename(tmp_path / "gone")
    assert not cached_ast_unit(
        "deleted", runner=lambda: AstUnitResult({}, ()), **kwargs
    ).cache_hit


def test_mutation_during_parse_is_rejected(tmp_path: Path) -> None:
    includes = tmp_path / "include"
    includes.mkdir()
    header = includes / "header.h"
    header.write_text("old", encoding="utf-8")

    def parse() -> AstUnitResult:
        header.write_text("new", encoding="utf-8")
        return AstUnitResult({"ok": True}, (header,))

    with pytest.raises(ValueError, match="changed during analysis"):
        cached_ast_unit(
            "race",
            identity={},
            input_files=[],
            include_roots=[includes],
            runner=parse,
            cache_dir=tmp_path / "cache",
        )


def test_lost_manifest_does_not_reuse_partial_dependency_key(tmp_path: Path) -> None:
    includes = tmp_path / "include"
    includes.mkdir()
    header = includes / "header.h"
    header.write_text("old", encoding="utf-8")
    kwargs: CacheCall = dict(
        identity={},
        input_files=[],
        include_roots=[includes],
        runner=lambda: AstUnitResult({"text": header.read_text()}, (header,)),
        cache_dir=tmp_path / "cache",
    )
    cached_ast_unit("lost", **kwargs)
    manifest = next((tmp_path / "cache").glob("*.json"))
    manifest.write_text("corrupt", encoding="utf-8")
    header.write_text("new", encoding="utf-8")
    result = cached_ast_unit("lost", **kwargs)
    assert not result.cache_hit
    assert result.payload == {"text": "new"}


def test_corrupt_result_is_a_miss(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    import zccache

    source = tmp_path / "source.cpp"
    source.write_text("source", encoding="utf-8")
    calls = []

    def parse() -> AstUnitResult:
        calls.append(True)
        return AstUnitResult({"violations": ["still a violation"]}, (source,))

    kwargs: CacheCall = dict(
        identity={},
        input_files=[source],
        include_roots=[],
        runner=parse,
        cache_dir=tmp_path / "cache",
    )
    cached_ast_unit("corruption", **kwargs)
    # Deliberately corrupt the storage boundary, not the semantic parser.
    monkeypatch.setattr(
        zccache, "exec_cached", lambda **_kwargs: b'{"complete":true,"payload":[]}'
    )
    result = cached_ast_unit("corruption", **kwargs)
    assert not result.cache_hit
    assert result.payload == {"violations": ["still a violation"]}
    assert len(calls) == 2


@pytest.mark.parametrize("changed", ["source", "header", "new_header"])
def test_shared_warm_snapshot_rejects_mutation_before_reporting(
    tmp_path: Path, changed: str
) -> None:
    source = tmp_path / "source.cpp"
    source.write_text("old", encoding="utf-8")
    includes = tmp_path / "include"
    includes.mkdir()
    header = includes / "header.h"
    header.write_text("old", encoding="utf-8")
    calls: list[bool] = []

    def parse() -> AstUnitResult:
        calls.append(True)
        return AstUnitResult({"violations": []}, (source, header))

    kwargs: CacheCall = dict(
        identity={},
        input_files=[source],
        include_roots=[includes],
        runner=parse,
        cache_dir=tmp_path / "cache",
    )
    cached_ast_unit("shared-warm", **kwargs)
    snapshot = AstInputSnapshot()
    assert cached_ast_unit("shared-warm", snapshot=snapshot, **kwargs).cache_hit
    snapshot.verify_final()
    if changed == "new_header":
        (includes / "optional.h").write_text("new", encoding="utf-8")
    else:
        modified = source if changed == "source" else header
        stat = modified.stat()
        modified.write_text("new", encoding="utf-8")
        os.utime(modified, ns=(stat.st_atime_ns, stat.st_mtime_ns))
    # Shared lookup hashes can still hit; the invocation must validate the
    # complete snapshot before reporting those results as current evidence.
    assert cached_ast_unit("shared-warm", snapshot=snapshot, **kwargs).cache_hit
    with pytest.raises(ValueError, match="changed during analysis"):
        snapshot.verify_final()
    assert len(calls) == 1
