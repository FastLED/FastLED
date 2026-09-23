"""Curated, mutually exclusive groups of example sketch directories.

Paths are relative to ``examples/``. Keep this inventory explicit so adding a
sketch requires a deliberate classification (checked by the test suite).
"""

import sys
from collections.abc import Iterable


GROUPS: dict[str, tuple[str, ...]] = {
    "Basic": (
        "AnalogOutput",
        "Apa102",
        "Blink",
        "BlinkParallel",
        "ColorPalette",
        "ColorTemperature",
        "FirstLight",
        "HSVTest",
        "Multiple/ArrayOfLedArrays",
        "Multiple/MirroringSample",
        "Multiple/MultiArrays",
        "Multiple/MultipleStripsInOneArray",
        "Noise",
        "NoisePlusPalette",
        "PinMode",
        "RGBSetDemo",
        "Spi",
        "XYMatrix",
    ),
    "Classic": (
        "Blur",
        "Cylon",
        "DemoReel100",
        "EaseInOut",
        "Fire2012",
        "Fire2012WithPalette",
        "MoodRing",
        "Pacifica",
        "Pride2015",
        "TwinkleFox",
        "Wave",
        "Wave2d",
    ),
    "Advanced": (
        "Animartrix",
        "AnimartrixRing",
        "Apa102HD",
        "Async",
        "Audio",
        "AudioInput",
        "AudioReactive",
        "BeatDetection",
        "Blur2d",
        "Chromancer",
        "Codec",
        "ColorBoost",
        "Corkscrew",
        "Downscale",
        "ElPanelReactive",
        "Esp8266Uart",
        "FestivalStick",
        "Fire2023",
        "FireCylinder",
        "FireMatrix",
        "FlowField",
        "HD107",
        "LuminescentGrand",
        "Luminova",
        "Multiple/ParallelOutputDemo",
        "MultipleEsp32SpiBuses",
        "OTA",
        "ParallelSPI",
        "Ports/PJRCSpectrumAnalyzer",
        "RGBW",
        "RGBWColorimetric",
        "RGBWEmulated",
        "RGBWW",
        "Remote",
        "Sailboat",
        "SmartMatrix",
        "SpecialDrivers/Adafruit/AdafruitBridge",
        "SpecialDrivers/ESP/DriverTest",
        "SpecialDrivers/RP/Parallel_IO",
        "SpecialDrivers/Teensy/ObjectFLED/TeensyMassiveParallel",
        "SpecialDrivers/Teensy/OctoWS2811/OctoWS2811",
        "SpecialDrivers/Teensy/OctoWS2811/OctoWS2811Demo",
        "WS2816",
        "XYPath",
        "hydropack",
    ),
    "Fx": (
        "Fx/FxCylon",
        "Fx/FxDemoReel100",
        "Fx/FxEngine",
        "Fx/FxFire2012",
        "Fx/FxGfx2Video",
        "Fx/FxLedmapper32x32",
        "Fx/FxNoisePlusPalette",
        "Fx/FxNoiseRing",
        "Fx/FxPacifica",
        "Fx/FxPride2015",
        "Fx/FxSdCard",
        "Fx/FxTwinkleFox",
        "Fx/FxWater",
        "Fx/FxWave2d",
        "Fx/Particles1d",
    ),
    "Experimental": (
        "Asio/Client",
        "Asio/ClientValidation",
        "Asio/Loopback",
        "Asio/RpcBidirectional",
        "Asio/RpcClient",
        "Asio/RpcServer",
        "Asio/Server",
        "AudioFftParity",
        "AudioUrl",
        "BlurBenchmark",
        "ColorProfile",
        "Json",
        "NoisePlayground",
        "Overclock",
        "PerfDisc",
        "Pintest",
        "RGBCalibrate",
        "RX",
        "SIMD",
        "Test",
        "UITest",
        "WasmScreenCoords",
        "wasm",
    ),
    "AutoResearch": ("AutoResearch",),
}


def select_examples(groups: Iterable[str]) -> list[str]:
    """Return the selected sketch directories in group order, once each."""
    selected: list[str] = []
    seen: set[str] = set()
    for group in groups:
        if group not in GROUPS:
            raise ValueError(f"Unknown example group: {group}")
        for example in GROUPS[group]:
            if example not in seen:
                selected.append(example)
                seen.add(example)
    return selected


def meson_metadata() -> str:
    """Emit sketch-directory to group assignments for Meson configuration."""
    return "\n".join(
        f"GROUP|examples/{example}|{group}"
        for group, examples in GROUPS.items()
        for example in examples
    )


if __name__ == "__main__":
    if sys.argv[1:] != ["--meson-metadata"]:
        raise SystemExit("usage: example_groups.py --meson-metadata")
    print(meson_metadata())
