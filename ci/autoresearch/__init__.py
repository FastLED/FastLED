"""FastLED autoresearch subpackage — hardware-in-the-loop test modules.

Package structure:
  runner.py       - Orchestrator: run(), main()
  args.py         - Args dataclass + parse_args()
  context.py      - RunContext, QuietContext, constants
  build_driver.py - BuildDriver protocol, FbuildDriver
  gpio.py         - GPIO pretest, pin discovery
  phases.py       - Pipeline phase functions
  ble.py          - BLE autoresearch
  decode.py       - Decode autoresearch
  net.py          - Network autoresearch
  ota.py          - OTA autoresearch
"""

from ci.autoresearch.args import Args
from ci.autoresearch.build_driver import (
    BuildDriver,
    FbuildDriver,
    select_build_driver,
)
from ci.autoresearch.context import QuietContext, RunContext
from ci.autoresearch.gpio import PinDiscoveryResult, run_gpio_pretest, run_pin_discovery
from ci.autoresearch.runner import main, run


__all__ = [
    "Args",
    "BuildDriver",
    "FbuildDriver",
    "PinDiscoveryResult",
    "QuietContext",
    "RunContext",
    "main",
    "run",
    "run_gpio_pretest",
    "run_pin_discovery",
    "select_build_driver",
]
