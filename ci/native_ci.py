"""Small, explicit native CI smoke inventory; full suites remain opt-in.

The ordinary PR/master budget is for the *whole event*, not each workflow.
Do not expand this list without measuring an exact-SHA PR and master event.
The full 311-unit/CI-Python suites still run with ``ci-full`` and at release.
"""

import argparse
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent

# The first target builds the shared fastled.so and test runner. Subsequent
# targets reuse that Meson build instead of compiling the 664-target full suite.
# channel_driver_uart is the one live test currently declared serial by Meson.
CPP_SMOKE = {
    "fastled_core": "tests/fastled_core.cpp",
    "fl_hsv2rgb_accuracy": "tests/fl/hsv2rgb_accuracy.cpp",
    "channel_driver_uart": "tests/platforms/esp/32/drivers/uart/channel_driver_uart.cpp",
    "channel_driver_rmt": "tests/platforms/esp/32/drivers/rmt/rmt_5/channel_driver_rmt.cpp",
}

# Representative color-reference/corpus coverage from #4359, plus selector
# and workflow-security guards. The ~1,500-test Python suite remains full-only.
PY_SMOKE = (
    "ci/tests/test_color_reference.py",
    "ci/tests/test_color_reference_corpus.py",
    "ci/tests/test_ci_labels.py",
    "ci/tests/test_native_ci_modes.py",
    "ci/tests/test_github_actions_security.py",
    "ci/tests/test_compile_example_sharding.py",
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=("cpp", "py"))
    args = parser.parse_args()
    entries = CPP_SMOKE if args.kind == "cpp" else PY_SMOKE
    # On Windows, print() translates LF to CRLF; Bash mapfile strips only LF
    # and passes a trailing CR into test discovery (FastLED #4543). Write to
    # the binary stream so the manifest protocol is exactly LF on every host.
    for entry in entries:
        sys.stdout.buffer.write(f"{entry}\n".encode("utf-8"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
