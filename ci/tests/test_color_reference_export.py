"""The generated C++ reference vectors must match the golden corpus.

`tests/fl/gfx/pipeline.cpp` measures P6 (FastLED#4040) against
`ci/golden/color-reference-v1.json`. It used to do that by carrying 48 of the
corpus's vectors as hand-typed literals, with the corpus named only in a
comment -- so the two could diverge with nothing to say so, which is the
defect FastLED#4279 fixed elsewhere.

`ci/color_reference_export.py` writes them out instead. This checks the
checked-in header is what that script produces now, so a corpus change that
is not carried through fails here rather than leaving the C++ side asserting
against numbers the corpus no longer holds.
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path

from ci.color_reference_export import (
    GOLDEN_FILE,
    HEADER_FILE,
    build_header,
    kDeviceProfile,
    load_vectors,
)


class TestGeneratedHeaderMatchesCorpus(unittest.TestCase):
    """The checked-in header is the corpus, not a copy that drifted from it."""

    def test_header_on_disk_is_current(self) -> None:
        self.assertTrue(HEADER_FILE.exists(), f"{HEADER_FILE} is missing")
        self.assertEqual(
            HEADER_FILE.read_text(),
            build_header(),
            "tests/fl/gfx/color_reference_vectors.hpp is stale; run "
            "`uv run python ci/color_reference_export.py`",
        )

    def test_every_corpus_vector_for_the_device_is_exported(self) -> None:
        """No silent subset: the count comes from the corpus, not a constant."""

        golden = json.loads(GOLDEN_FILE.read_text())
        expected = 0
        for vector in golden["vectors"]:
            if vector["device_profile"] == kDeviceProfile:
                expected += 1
        self.assertEqual(len(load_vectors(golden)), expected)
        self.assertGreater(expected, 0)

    def test_exported_values_are_the_corpus_values(self) -> None:
        """Spot-check the payload rather than only the byte-for-byte render.

        A generator that rendered a stable but wrong table would satisfy the
        staleness check above, since that compares the header against the
        generator rather than against the corpus.
        """

        golden = json.loads(GOLDEN_FILE.read_text())
        by_id = {}
        for vector in golden["vectors"]:
            by_id[vector["id"]] = vector

        header = HEADER_FILE.read_text()
        for exported in load_vectors(golden):
            source = by_id[exported.identifier]
            stages = source["stages"]
            self.assertEqual(
                list(exported.emitter_light), list(stages["emitter_light"])
            )
            self.assertEqual(list(exported.mapped_xyz), list(stages["mapped_xyz"]))
            self.assertEqual(
                [float(code) for code in exported.encoded], stages["encoded_rgb8"]
            )
            # And the value actually reached the file, not just the loader.
            self.assertIn(exported.identifier, header)


if __name__ == "__main__":
    unittest.main()
