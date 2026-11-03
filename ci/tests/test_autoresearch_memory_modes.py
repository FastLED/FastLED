"""Memory benches must select explicit, reproducible AutoResearch modes."""

import pytest

from ci.autoresearch import memory_bench
from ci.autoresearch.args import Args


@pytest.mark.parametrize("mode", ["typed", "runtime"])
def test_driver_binding_mode(mode: str) -> None:
    args = Args.parse_args(["esp32s3", "--driver-binding", mode])
    assert args.driver_binding == mode


def test_spi_streaming_mode() -> None:
    args = Args.parse_args(["esp32s3", "--spi-streaming"])
    assert args.spi_streaming


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
