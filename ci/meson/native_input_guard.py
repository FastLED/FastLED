"""Invalidate native objects when changed source content keeps an old mtime."""

import hashlib
import json
import os
from dataclasses import dataclass, field
from pathlib import Path

from running_process import PIPE, RunningProcess

from ci.early_exit_cache import NATIVE_SOURCE_EXTS


@dataclass
class ObjectInputState:
    """Inputs changed since this particular object was last compiled."""

    signature: list[int]
    sha256: str
    pending: dict[str, str | None] = field(default_factory=dict)


@dataclass
class NativeInputState:
    """Content observed before compilation; publish only after success."""

    build_dir: Path
    inputs: dict[str, str]
    changed: bool
    objects: dict[str, ObjectInputState] = field(default_factory=dict)
    invalidated: int = 0


def _object_signature(path: Path) -> list[int]:
    stat = path.stat()
    return [stat.st_mtime_ns, stat.st_ctime_ns, stat.st_ino, stat.st_size]


def _file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _ordinary_objects(build_dir: Path) -> list[Path]:
    return [
        path
        for path in build_dir.rglob("*")
        if path.suffix.lower() in {".o", ".obj"} and path.is_file()
    ]


def _load_baseline(
    marker: Path,
) -> tuple[dict[str, str], dict[str, ObjectInputState]] | None:
    """Reject incomplete or malformed tracking rather than trust its objects."""
    try:
        data = json.loads(marker.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None
    if not isinstance(data, dict) or data.get("version") != 2:
        return None
    inputs = data.get("inputs")
    objects = data.get("objects")
    if not isinstance(inputs, dict) or not isinstance(objects, dict):
        return None
    if not all(isinstance(k, str) and isinstance(v, str) for k, v in inputs.items()):
        return None
    records: dict[str, ObjectInputState] = {}
    for name, record in objects.items():
        if not isinstance(name, str) or not isinstance(record, dict):
            return None
        signature, pending = record.get("signature"), record.get("pending")
        digest = record.get("sha256")
        if (
            not isinstance(signature, list)
            or len(signature) != 4
            or not all(isinstance(value, int) for value in signature)
            or not isinstance(digest, str)
            or len(digest) != 64
            or not isinstance(pending, dict)
            or not all(
                isinstance(k, str) and (v is None or isinstance(v, str))
                for k, v in pending.items()
            )
        ):
            return None
        records[name] = ObjectInputState(signature, digest, pending)
    return inputs, records


def prepare_native_inputs(build_dir: Path, project_root: Path) -> NativeInputState:
    """Compare project content with this build directory's successful baseline."""
    result = RunningProcess.run(
        [
            "git",
            "-C",
            str(project_root),
            "ls-files",
            "-z",
            "--cached",
            "--others",
            "--exclude-standard",
            "--",
            "src",
            "tests",
            "examples",
        ],
        stdout=PIPE,
        stderr=PIPE,
        encoding="utf-8",
        errors="replace",
        check=True,
        timeout=30,
    )
    inputs: dict[str, str] = {}
    mtimes: dict[str, int] = {}
    for name in set(result.stdout.split("\0")):
        path = project_root / name
        if path.suffix.lower() not in NATIVE_SOURCE_EXTS or not path.is_file():
            continue
        inputs[name] = _file_hash(path)
        mtimes[name] = path.stat().st_mtime_ns
    baseline = _load_baseline(build_dir / ".native-inputs.json")
    previous, records = baseline if baseline is not None else ({}, {})
    state = NativeInputState(build_dir, inputs, inputs != previous)
    changed = {
        name
        for name in inputs.keys() | (previous or {}).keys()
        if inputs.get(name) != previous.get(name)
    }
    for path in _ordinary_objects(build_dir):
        name = path.relative_to(build_dir).as_posix()
        signature = _object_signature(path)
        record = records.get(name)
        digest = (
            record.sha256
            if record and record.signature == signature
            else _file_hash(path)
        )
        # A partial build must not certify the ordinary objects it did not
        # rebuild. Keep their old input hashes until their output changes.
        pending = dict(record.pending) if record else {}
        for input_name in changed:
            pending.setdefault(input_name, previous.get(input_name))
        pending = {
            key: old_hash
            for key, old_hash in pending.items()
            if inputs.get(key) != old_hash
        }
        # Newer edits remain Ninja's dependency-directed rebuilds. Only
        # changed inputs hidden by timestamps require our intervention.
        unsafe = (
            record is None
            or record.sha256 != digest
            or any(key not in inputs or mtimes[key] <= signature[0] for key in pending)
        )
        if unsafe:
            path.unlink()
            state.invalidated += 1
        else:
            state.objects[name] = ObjectInputState(signature, digest, pending)
    # Pending entries can belong to unrelated or nondefault objects. A target
    # shortcut has its own successful-build content hash, so those entries
    # must not globally disable it. A hidden edit still invalidates its object
    # above, which blocks the shortcut before Ninja can miss that edit.
    state.changed = state.changed or bool(state.invalidated)
    return state


def save_native_inputs(state: NativeInputState) -> None:
    """Atomically certify the content used by a successful native compile."""
    marker = state.build_dir / ".native-inputs.json"
    temporary = marker.with_suffix(f".json.{os.getpid()}.tmp")
    objects: dict[str, dict[str, object]] = {}
    for path in _ordinary_objects(state.build_dir):
        name = path.relative_to(state.build_dir).as_posix()
        signature = _object_signature(path)
        previous = state.objects.get(name)
        digest = (
            previous.sha256
            if previous and previous.signature == signature
            else _file_hash(path)
        )
        pending = previous.pending if previous and previous.sha256 == digest else {}
        objects[name] = {"signature": signature, "sha256": digest, "pending": pending}
    temporary.write_text(
        json.dumps(
            {"version": 2, "inputs": state.inputs, "objects": objects}, sort_keys=True
        ),
        encoding="utf-8",
    )
    temporary.replace(marker)
