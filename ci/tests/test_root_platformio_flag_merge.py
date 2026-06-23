"""Tests for ``get_root_platformio_build_flags`` (issue #2664).

The ``bash compile --backend platformio`` path synthesises a fresh PIO
project from ``ci/boards.py`` and historically discarded any
``[env:<board>].build_flags`` defined in the user's root ``platformio.ini``.
``get_root_platformio_build_flags`` reads the root file and returns those
flags so ``_init_platformio_build`` in ``ci/compiler/pio.py`` can append
them to the synthesised env.
"""

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from ci.compiler.build_config import get_root_platformio_build_flags


class TestRootPlatformioBuildFlagsMerge(unittest.TestCase):
    """Exercise the root-platformio.ini → synthesised-env flag bridge."""

    def _write_root_ini(self, root: Path, body: str) -> None:
        (root / "platformio.ini").write_text(body)

    def test_missing_file_returns_empty(self) -> None:
        with TemporaryDirectory() as tmp:
            self.assertEqual(
                get_root_platformio_build_flags("esp32c6", Path(tmp)),
                [],
            )

    def test_no_matching_env_returns_empty(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            self._write_root_ini(
                root,
                "[env:uno]\nplatform = atmelavr\nbuild_flags = -DUNO_ONLY\n",
            )
            self.assertEqual(
                get_root_platformio_build_flags("esp32c6", root),
                [],
            )

    def test_direct_build_flags_returned(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            self._write_root_ini(
                root,
                """[env:esp32c6]
platform = espressif32
build_flags =
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -D PIN_DATA=21
""",
            )
            flags = get_root_platformio_build_flags("esp32c6", root)
            self.assertIn("-D ARDUINO_USB_MODE=1", flags)
            self.assertIn("-D ARDUINO_USB_CDC_ON_BOOT=1", flags)
            self.assertIn("-D PIN_DATA=21", flags)

    def test_extends_inheritance_is_resolved(self) -> None:
        """A child env that ``extends`` a base env inherits the base's flags."""
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            self._write_root_ini(
                root,
                """[env:generic-esp]
platform = espressif32
build_flags =
    -DDEBUG
    -DCORE_DEBUG_LEVEL=5

[env:esp32c6]
extends = env:generic-esp
build_flags =
    ${env:generic-esp.build_flags}
    -D ARDUINO_USB_CDC_ON_BOOT=1
""",
            )
            flags = get_root_platformio_build_flags("esp32c6", root)
            # Base flags must appear...
            self.assertIn("-DDEBUG", flags)
            self.assertIn("-DCORE_DEBUG_LEVEL=5", flags)
            # ...alongside the child's own additions.
            self.assertIn("-D ARDUINO_USB_CDC_ON_BOOT=1", flags)

    def test_malformed_file_returns_empty(self) -> None:
        """A broken root ini must never block a build — it returns []."""
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            # Duplicate section + no header — configparser will raise.
            self._write_root_ini(
                root,
                "= = = not actually ini = = =\n[env:esp32c6\nbuild_flags = -DX\n",
            )
            self.assertEqual(
                get_root_platformio_build_flags("esp32c6", root),
                [],
            )


if __name__ == "__main__":
    unittest.main()
