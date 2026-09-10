from pathlib import Path

import pytest
import yaml

from ci.compiler.argument_parser import CompilationArgumentParser


ROOT = Path(__file__).resolve().parents[2]

# The shard count the esp32s3 workflow is expected to run. Declared here rather
# than read back out of the workflow so the assertions below stay meaningful:
# changing the workflow's fan-out has to be a deliberate edit in both places,
# not something that silently redefines its own expectation.
SHARD_COUNT = 3


def test_esp32s3_workflow_names_each_shard_explicitly() -> None:
    workflow_path = ROOT / ".github/workflows/build_esp32s3.yml"
    workflow = yaml.safe_load(workflow_path.read_text(encoding="utf-8"))
    build_job = workflow["jobs"]["build"]

    assert build_job["name"] == (
        f"ESP32-S3 examples shard ${{{{ matrix.shard_index }}}} ({SHARD_COUNT} total)"
    )
    assert build_job["strategy"]["matrix"]["shard_index"] == list(range(SHARD_COUNT))
    assert build_job["with"]["args"] == (
        "esp32s3 all --shard-index ${{ matrix.shard_index }} "
        f"--shard-count {SHARD_COUNT}"
    )


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
                str(SHARD_COUNT),
            ]
        ).examples
        for index in range(SHARD_COUNT)
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
