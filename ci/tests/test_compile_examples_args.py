from pathlib import Path

from ci.compiler.argument_parser import CompilationArgumentParser


ROOT = Path(__file__).resolve().parents[2]


def test_examples_option_and_positionals_are_merged() -> None:
    # `--examples A B C` parses A as the option value and B C as positionals;
    # all three must be compiled (#4805: RGBW was silently dropped).
    config = CompilationArgumentParser(ROOT).parse(
        ["atmega8a", "--examples", "RGBW", "RGBWEmulated", "Blink"]
    )
    assert config.examples == ["RGBW", "RGBWEmulated", "Blink"]


def test_comma_list_and_duplicates() -> None:
    config = CompilationArgumentParser(ROOT).parse(
        ["uno", "--examples", "Blink,RGBW", "Blink"]
    )
    assert config.examples == ["Blink", "RGBW"]
