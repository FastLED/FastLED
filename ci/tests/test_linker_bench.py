"""Focused safety and ordering tests for native link replay."""

from pathlib import Path
from unittest.mock import patch

import pytest

from ci.tools.linker_bench import (  # type: ignore[import-not-found]
    LinkCommand,
    classify_reld,
    discover_links,
    replay_argv,
    scratch_output,
    select_links,
    sweep_totals,
    target_major_order,
)


@pytest.mark.parametrize("existing_path", [None, "/opt/reld"])
def test_replay_changes_only_linker_and_output(
    tmp_path: Path, existing_path: str | None
) -> None:
    original = LinkCommand(
        "tests/sample.so",
        (
            "clang++",
            "-o",
            "tests/sample.so",
            "tests/sample.so.p/a.o",
            "-fuse-ld=lld",
            *((f"--ld-path={existing_path}",) if existing_path else ()),
            "-shared",
        ),
    )
    output = tmp_path / "sample.so"
    result = replay_argv(original, "/opt/mold", output)
    assert result == [
        "clang++",
        "-fuse-ld=lld",
        "--ld-path=/opt/mold",
        "-o",
        str(output),
        "tests/sample.so.p/a.o",
        "-shared",
    ]
    assert "/opt/reld" not in " ".join(result)
    assert original.argv[2] == "tests/sample.so"


def test_replay_rejects_ambiguous_link_command(tmp_path: Path) -> None:
    command = LinkCommand(
        "tests/sample.so", ("clang++", "-o", "tests/sample.so", "a.o")
    )
    with pytest.raises(ValueError, match="linker selection"):
        replay_argv(command, "/opt/mold", tmp_path / "sample.so")


def test_replay_removes_every_stale_linker_selector(tmp_path: Path) -> None:
    command = LinkCommand(
        "tests/sample.so",
        (
            "clang++",
            "-fuse-ld=lld",
            "--ld-path=/opt/reld",
            "-fuse-ld=mold",
            "--ld-path",
            "/opt/older-linker",
            "-o",
            "tests/sample.so",
            "a.o",
        ),
    )
    argv = replay_argv(command, "/opt/mold", tmp_path / "sample.so")
    assert argv == [
        "clang++",
        "-fuse-ld=lld",
        "--ld-path=/opt/mold",
        "-o",
        str(tmp_path / "sample.so"),
        "a.o",
    ]


def test_replay_bypasses_link_cache(tmp_path: Path) -> None:
    command = LinkCommand(
        "tests/sample.so",
        (
            "/venv/bin/zccache",
            "/toolchain/clang++",
            "-o",
            "tests/sample.so",
            "a.o",
            "-fuse-ld=lld",
        ),
    )
    argv = replay_argv(command, "/opt/mold", tmp_path / "sample.so")
    assert argv[0] == "/toolchain/clang++"
    assert command.argv[0] == "/venv/bin/zccache"


def test_selection_requires_core_and_tests() -> None:
    links = [
        LinkCommand("ci/meson/native/fastled.so", ("clang++",)),
        LinkCommand("tests/a.so", ("clang++",)),
        LinkCommand("tests/b.so", ("clang++",)),
    ]
    assert [link.target for link in select_links(links, None)] == [
        "ci/meson/native/fastled.so",
        "tests/a.so",
        "tests/b.so",
    ]
    with pytest.raises(ValueError, match="No test shared objects"):
        select_links(links[:1], None)
    with pytest.raises(ValueError, match="not found"):
        select_links(links, "missing")


def test_selection_limits_sorted_tests_and_opts_into_examples() -> None:
    links = [
        LinkCommand("tests/b.so", ("clang++",)),
        LinkCommand("examples/example-Z.so", ("clang++",)),
        LinkCommand("ci/meson/native/fastled.so", ("clang++",)),
        LinkCommand("tests/a.so", ("clang++",)),
        LinkCommand("examples/example-A.so", ("clang++",)),
    ]
    assert [link.target for link in select_links(links, None, max_tests=1)] == [
        "ci/meson/native/fastled.so",
        "tests/a.so",
    ]
    assert [
        link.target
        for link in select_links(links, None, include_examples=True, max_tests=1)
    ] == [
        "ci/meson/native/fastled.so",
        "tests/a.so",
        "examples/example-A.so",
        "examples/example-Z.so",
    ]
    with pytest.raises(ValueError, match="positive"):
        select_links(links, None, max_tests=0)
    with pytest.raises(ValueError, match="No example"):
        select_links([links[0], links[2]], None, include_examples=True)


def test_reld_engine_classification() -> None:
    assert classify_reld("reld: engine=reld (native, reason=default)") == "native"
    assert classify_reld("reld: engine=lld (bridge, reason=flag:--icf)") == "bridge"
    assert classify_reld("no engine line") == "unclassified"


def test_discovery_explicitly_requests_nondefault_examples(tmp_path: Path) -> None:
    (tmp_path / "build.ninja").write_text(
        "build ci/meson/native/fastled.so: cpp_LINKER core.o\n"
        "build tests/a.so: cpp_LINKER a.o\n"
        "build examples/example-A.so: cpp_LINKER example.o\n",
        encoding="utf-8",
    )
    with patch("ci.tools.linker_bench._run") as run:
        run.return_value = (
            "clang++ -o ci/meson/native/fastled.so core.o -fuse-ld=lld\n"
            "clang++ -o tests/a.so a.o -fuse-ld=lld\n"
            "clang++ -o examples/example-A.so example.o -fuse-ld=lld\n"
        )
        links = discover_links(tmp_path)
    assert [link.target for link in links] == [
        "ci/meson/native/fastled.so",
        "examples/example-A.so",
        "tests/a.so",
    ]
    assert run.call_args.args[0] == [
        "ninja",
        "-t",
        "commands",
        "ci/meson/native/fastled.so",
        "examples/example-A.so",
        "tests/a.so",
    ]


def test_target_major_order_rotates_per_target_and_repetition() -> None:
    assert target_major_order(["core", "test"], ["lld", "mold", "wild"], 2) == [
        (0, "core", ["lld", "mold", "wild"]),
        (0, "test", ["mold", "wild", "lld"]),
        (1, "core", ["wild", "lld", "mold"]),
        (1, "test", ["lld", "mold", "wild"]),
    ]


def test_sweep_totals_sum_link_samples_by_category() -> None:
    samples = [
        {"repetition": 0, "target": "ci/meson/native/fastled.so", "seconds": 1.0},
        {"repetition": 0, "target": "tests/a.so", "seconds": 2.0},
        {"repetition": 0, "target": "examples/example-A.so", "seconds": 3.0},
        {"repetition": 1, "target": "ci/meson/native/fastled.so", "seconds": 4.0},
        {"repetition": 1, "target": "tests/a.so", "seconds": 5.0},
        {"repetition": 1, "target": "examples/example-A.so", "seconds": 6.0},
    ]
    assert sweep_totals(samples, 2) == [
        {
            "repetition": 0,
            "seconds": 6.0,
            "core_seconds": 1.0,
            "modules_seconds": 2.0,
            "examples_seconds": 3.0,
        },
        {
            "repetition": 1,
            "seconds": 15.0,
            "core_seconds": 4.0,
            "modules_seconds": 5.0,
            "examples_seconds": 6.0,
        },
    ]


def test_scratch_output_isolated_per_repetition_and_candidate(tmp_path: Path) -> None:
    target = "ci/meson/native/fastled.so"
    first = scratch_output(tmp_path, 0, "lld", target)
    second = scratch_output(tmp_path, 0, "mold", target)
    third = scratch_output(tmp_path, 1, "lld", target)
    assert first.name == second.name == third.name == "fastled.so"
    assert len({first.parent, second.parent, third.parent}) == 3
    assert all(path.is_relative_to(tmp_path) for path in (first, second, third))
