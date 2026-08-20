from pathlib import Path

import pytest

from ci.compiler.argument_parser import CompilationArgumentParser


ROOT = Path(__file__).resolve().parents[2]


def test_all_example_shards_are_disjoint_and_exhaustive() -> None:
    parser = CompilationArgumentParser(ROOT)
    all_examples = parser._discover_all_examples()

    shards = [
        parser.parse(
            [
                "esp32s3",
                "all",
                "--shard-index",
                str(index),
                "--shard-count",
                "8",
            ]
        ).examples
        for index in range(8)
    ]

    flattened = [example for shard in shards for example in shard]
    assert sorted(flattened) == all_examples
    assert len(flattened) == len(set(flattened))
    assert max(map(len, shards)) - min(map(len, shards)) <= 1


@pytest.mark.parametrize(
    "args, message",
    [
        (["esp32s3", "all", "--shard-count", "8"], "must be used together"),
        (
            ["esp32s3", "Blink", "--shard-index", "0", "--shard-count", "8"],
            "requires the 'all' keyword",
        ),
        (
            ["esp32s3", "all", "--shard-index", "0", "--shard-count", "0"],
            "must be a positive integer",
        ),
        (
            ["esp32s3", "all", "--shard-index", "8", "--shard-count", "8"],
            "must be in the range 0..7",
        ),
    ],
)
def test_invalid_example_shards_are_rejected(args: list[str], message: str) -> None:
    parser = CompilationArgumentParser(ROOT)

    with pytest.raises(ValueError, match=message):
        parser.parse(args)
