"""Memory benches must select explicit, reproducible AutoResearch modes."""

import os
from pathlib import Path

import pytest

from ci.autoresearch import memory_bench
from ci.autoresearch.args import Args


@pytest.mark.parametrize("mode", ["typed", "runtime"])
def test_driver_binding_mode(mode: str) -> None:
    args = Args.parse_args(["esp32s3", "--driver-binding", mode])
    assert args.driver_binding == mode
    assert args.strip_sizes is None
    assert args.frames is None
    assert args.chipset == "ws2812"
    assert args.rp_spi_chipset == "apa102"


def test_spi_streaming_mode() -> None:
    args = Args.parse_args(["esp32s3", "--spi-streaming"])
    assert args.spi_streaming
    assert args.strip_sizes is None
    assert args.frames is None
    assert args.chipset == "ws2812"
    assert args.rp_spi_chipset == "apa102"


def test_memory_modes_are_mutually_exclusive() -> None:
    with pytest.raises(SystemExit):
        Args.parse_args(["esp32s3", "--driver-binding", "typed", "--spi-streaming"])


@pytest.mark.parametrize(
    "memory_mode", [["--spi-streaming"], ["--driver-binding", "typed"]]
)
@pytest.mark.parametrize(
    "other_mode",
    [
        ["--rmt"],
        ["--spi"],
        ["--simd"],
        ["--math"],
        ["--coroutine"],
        ["--decode", "example.json"],
        ["--rpc-smoke"],
        ["--static-init"],
        ["--static-init", "--static-init-no-lto"],
        ["--net-peer"],
        ["--watchdog-soak"],
        ["--spi-provider-native"],
        ["--legacy"],
        ["--embedded-fs"],
        ["--parallel"],
        ["--tight-timing"],
    ],
)
def test_memory_mode_does_not_silently_skip_other_modes(
    memory_mode: list[str],
    other_mode: list[str],
) -> None:
    with pytest.raises(SystemExit):
        Args.parse_args(["esp32s3", *memory_mode, *other_mode])


@pytest.mark.parametrize(
    "peak_dma, measured, accepted",
    [
        (
            4576,
            True,
            False,
        ),  # Matched full-frame baseline exceeds one-third frame storage.
        (1044, True, True),  # SPI3 byte-exact streaming hardware measurement.
        (1044, False, False),  # A missing watermark cannot establish a memory saving.
    ],
)
def test_spi_memory_acceptance_requires_measured_threefold_reduction(
    peak_dma: int,
    measured: bool,
    accepted: bool,
) -> None:
    gate = getattr(memory_bench, "spi_memory_within_budget", None)
    assert callable(gate), "SPI AutoResearch must enforce its measured memory budget"
    assert (
        gate(
            {
                "frameBytes": 4132,
                "peakDmaBytes": peak_dma,
                "heapWatermarkMeasured": measured,
            }
        )
        is accepted
    )


@pytest.mark.parametrize(
    "memory_mode",
    [
        ["--driver-binding", "typed"],
        ["--driver-binding", "runtime"],
        ["--spi-streaming"],
    ],
)
@pytest.mark.parametrize(
    "workload_option",
    [
        ["--strip-sizes", "100"],
        ["--strip-sizes", ""],
        ["--frames", "1"],
        ["--frames", "0"],
        ["--lanes", "1"],
        ["--lane-counts", "100,200"],
        ["--worker-core", "1"],
        ["--lane-pins", "1,3,4"],
        ["--color-pattern", "ff0000"],
        ["--tx-pin", "4"],
        ["--tx-pin", "0"],
        ["--rx-pin", "2"],
        ["--rx-pin", "0"],
        ["--chipset", "ws2814"],
        ["--chipset", "ws2812"],
        ["--rp-spi-chipset", "sk9822"],
        ["--rp-spi-chipset", "apa102"],
    ],
)
def test_memory_bench_rejects_explicit_ignored_workload_options(
    memory_mode: list[str],
    workload_option: list[str],
) -> None:
    with pytest.raises(SystemExit):
        Args.parse_args(["esp32s3", *memory_mode, *workload_option])


def test_memory_bench_provenance_uses_deployed_release_not_newer_debug(
    tmp_path: Path,
) -> None:
    release = tmp_path / ".fbuild/build/release/firmware.elf"
    debug = tmp_path / ".fbuild/build/debug/firmware.elf"
    release.parent.mkdir(parents=True)
    debug.parent.mkdir(parents=True)
    release.write_bytes(b"deployed release")
    debug.write_bytes(b"unrelated newer debug")
    os.utime(release, (100, 100))
    os.utime(debug, (200, 200))
    deployed_elf = getattr(memory_bench, "_deployed_elf", None)
    assert callable(deployed_elf), (
        "Benchmark provenance must select the deployed release artifact"
    )
    assert deployed_elf(tmp_path) == release


def test_memory_bench_provenance_rejects_missing_release_even_with_debug(
    tmp_path: Path,
) -> None:
    debug = tmp_path / ".fbuild/build/debug/firmware.elf"
    debug.parent.mkdir(parents=True)
    debug.write_bytes(b"not deployed")
    deployed_elf = getattr(memory_bench, "_deployed_elf", None)
    assert callable(deployed_elf), (
        "Benchmark provenance must reject a missing deployed release artifact"
    )
    with pytest.raises(FileNotFoundError, match="release/firmware.elf"):
        deployed_elf(tmp_path)


def test_default_chipset_choices_are_preserved() -> None:
    args = Args.parse_args(["esp32s3"])
    assert args.chipset == "ws2812"
    assert args.rp_spi_chipset == "apa102"


def test_non_memory_chipset_choices_are_preserved() -> None:
    args = Args.parse_args(
        ["esp32s3", "--chipset", "ucs7604", "--rp-spi-chipset", "sk9822"]
    )
    assert args.chipset == "ucs7604"
    assert args.rp_spi_chipset == "sk9822"
