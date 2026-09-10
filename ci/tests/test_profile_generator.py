"""The artifact -> C++ gate has to refuse, not just render (P3, #4037).

The registry deliberately holds catalog records whose optical data cannot
support an `EmitterProfile`. Both parts mirrored into this repo are exactly
that, so the generator's refusal path is the one that runs in production
today -- and a generator that quietly emitted something for them would be
manufacturing precision the datasheets do not publish.
"""

from __future__ import annotations

import hashlib
import json
import unittest
from pathlib import Path

from ci import generate_profile_header
from ci.color_profile_generator import (
    TOPOLOGY_CHANNELS,
    Admission,
    Refusal,
    admit,
    alias_name,
    load_artifacts,
    parse_artifact,
    render_header,
    symbol_name,
)


REPO_ROOT = Path(__file__).resolve().parents[2]
MIRRORED = REPO_ROOT / "ci" / "golden" / "profiles"
FIXTURES = REPO_ROOT / "ci" / "tests" / "fixtures" / "profiles"
GENERATED = REPO_ROOT / "tests" / "fl" / "gfx" / "colorimetric_response_profiles.hpp"


def _decide(directory: Path) -> list[object]:
    decisions: list[object] = []
    for artifact in load_artifacts(directory):
        decisions.append(admit(artifact))
    return decisions


class TestTheSetUnderTestIsNotEmpty(unittest.TestCase):
    def test_both_populations_exist(self: "TestTheSetUnderTestIsNotEmpty") -> None:
        # Every assertion below is quantified over one of these two sets. If
        # either were empty its assertions would pass without testing.
        mirrored = _decide(MIRRORED)
        fixtures = _decide(FIXTURES)
        self.assertGreaterEqual(len(mirrored), 2)
        self.assertGreaterEqual(len(fixtures), 1)
        self.assertTrue(any(isinstance(d, Refusal) for d in mirrored))
        self.assertTrue(any(isinstance(d, Admission) for d in fixtures))


class TestRefusal(unittest.TestCase):
    def test_every_mirrored_artifact_is_refused(self: "TestRefusal") -> None:
        for decision in _decide(MIRRORED):
            with self.subTest(profile=getattr(decision, "profile_id", decision)):
                self.assertIsInstance(decision, Refusal)

    def test_the_refusal_names_the_absent_top_level_flag(self: "TestRefusal") -> None:
        # The schema's rule is that an absent flag means false *even when
        # channels carry usable observations*. Inferring admissibility from
        # channel data is exactly how a catalog record becomes a fake profile,
        # so the diagnostic has to name the flag rather than only the channels.
        # Matching on the word `runtime_admissible` alone is not enough: the
        # per-channel message contains it too, so that form passes even for a
        # generator that dropped the top-level check. Match the phrase only
        # the top-level reason carries.
        for decision in _decide(MIRRORED):
            assert isinstance(decision, Refusal)
            with self.subTest(profile=decision.profile_id):
                self.assertTrue(
                    any("absent counts as false" in r for r in decision.reasons),
                    msg=f"reasons were {decision.reasons}",
                )

    def test_admissible_channels_do_not_imply_an_admissible_artifact(
        self: "TestRefusal",
    ) -> None:
        """The rule the whole gate rests on, tested on its own.

        Taking the admissible fixture and deleting only the top-level flag
        must still refuse. Asserting instead that some refusal *mentions*
        `runtime_admissible` is not enough -- the per-channel message contains
        that word too, so a generator that dropped the top-level check
        entirely would pass such a test while happily generating profiles for
        catalog records.
        """

        payload = json.loads(
            (FIXTURES / "fixture-rgb-none-synthetic-r1.profile.json").read_bytes()
        )
        self.assertIsInstance(
            admit(parse_artifact(json.dumps(payload).encode("utf-8"), "intact")),
            Admission,
        )

        del payload["runtime_admissible"]
        decision = admit(
            parse_artifact(json.dumps(payload).encode("utf-8"), "flagless")
        )
        self.assertIsInstance(decision, Refusal)
        assert isinstance(decision, Refusal)
        self.assertTrue(
            any("absent counts as false" in reason for reason in decision.reasons),
            msg=f"reasons were {decision.reasons}",
        )

    def test_a_range_observation_is_not_accepted_as_a_value(
        self: "TestRefusal",
    ) -> None:
        # The schema keeps min/typical/max ranges precisely so a table is not
        # collapsed into an invented midpoint. Accepting a range here would
        # re-introduce the invention one layer up.
        ranged_channels: list[dict[str, object]] = []
        for name in ("red", "green", "blue"):
            ranged_channels.append(
                {
                    "name": name,
                    "runtime_admissible": True,
                    "chromaticity_range": {"typical": {"x": 0.3, "y": 0.3}},
                    "relative_y_range": {"typical": {"value": 0.3}},
                }
            )
        payload = {
            "schema_version": "1.0",
            "profile_id": "ranged/rgb/none/r1",
            "runtime_admissible": True,
            "topology": "rgb",
            "photometric": {"channels": ranged_channels},
        }
        decision = admit(parse_artifact(json.dumps(payload).encode("utf-8"), "ranged"))
        self.assertIsInstance(decision, Refusal)
        assert isinstance(decision, Refusal)
        self.assertTrue(any("chromaticity" in r for r in decision.reasons))

    def test_the_channel_set_must_match_the_topology(self: "TestRefusal") -> None:
        short_channels: list[dict[str, object]] = []
        for name in ("red", "green", "blue"):
            short_channels.append(
                {
                    "name": name,
                    "runtime_admissible": True,
                    "chromaticity": {"x": 0.3, "y": 0.3},
                    "relative_y": {"value": 0.3},
                }
            )
        payload = {
            "schema_version": "1.0",
            "profile_id": "short/rgbw/none/r1",
            "runtime_admissible": True,
            "topology": "rgbw",  # requires a white channel that is not here
            "photometric": {"channels": short_channels},
        }
        decision = admit(parse_artifact(json.dumps(payload).encode("utf-8"), "short"))
        self.assertIsInstance(decision, Refusal)
        assert isinstance(decision, Refusal)
        self.assertTrue(any("requires exactly" in r for r in decision.reasons))

    def test_an_unknown_schema_major_raises_rather_than_refusing(
        self: "TestRefusal",
    ) -> None:
        # A future-major artifact is a tooling problem, not a statement about
        # the part's optics. Returning a refusal would let a schema bump read
        # as "this LED is not characterised".
        payload = {"schema_version": "2.0", "profile_id": "future/rgb/none/r1"}
        with self.assertRaises(ValueError) as caught:
            parse_artifact(json.dumps(payload).encode("utf-8"), "future")
        self.assertIn("unsupported major", str(caught.exception))


class TestAdmissionAndRendering(unittest.TestCase):
    def test_the_fixture_is_admitted(self: "TestAdmissionAndRendering") -> None:
        decisions = _decide(FIXTURES)
        self.assertTrue(all(isinstance(d, Admission) for d in decisions))

    def test_rendering_names_every_refused_part(
        self: "TestAdmissionAndRendering",
    ) -> None:
        # A generated file that silently omits a part reads as "not
        # catalogued". Naming it and saying why stops the next person
        # re-deriving the same dead end from the same PDF.
        rendered = GENERATED.read_text(encoding="utf-8")
        for decision in _decide(MIRRORED):
            assert isinstance(decision, Refusal)
            with self.subTest(profile=decision.profile_id):
                self.assertIn(decision.profile_id, rendered)

    def test_the_header_records_the_artifact_hash(
        self: "TestAdmissionAndRendering",
    ) -> None:
        rendered = GENERATED.read_text(encoding="utf-8")
        for artifact in load_artifacts(FIXTURES):
            with self.subTest(profile=artifact.profile_id):
                self.assertIn(artifact.content_sha256, rendered)
        self.assertIn(generate_profile_header.DATASHEETS_COMMIT, rendered)

    def test_symbols_keep_the_report_and_aliases_drop_it(
        self: "TestAdmissionAndRendering",
    ) -> None:
        # Two reports on one part are different profiles, so a versioned
        # symbol has to keep the report ID; the floating alias is what drops
        # it and is allowed to advance.
        self.assertEqual(
            symbol_name("ws2812b/5050/none/datasheet-r1"),
            "WS2812B_5050_NONE_DATASHEET_R1",
        )
        self.assertEqual(
            alias_name("ws2812b/5050/none/datasheet-r1"), "WS2812B_5050_NONE"
        )
        self.assertNotEqual(
            symbol_name("ws2812b/5050/none/datasheet-r1"),
            symbol_name("ws2812b/5050/none/datasheet-r2"),
        )


class TestRenderableTopologies(unittest.TestCase):
    def _white_artifact(self: "TestRenderableTopologies", topology: str) -> object:
        channels = TOPOLOGY_CHANNELS[topology]
        channel_records: list[dict[str, object]] = []
        for name in channels:
            channel_records.append(
                {
                    "name": name,
                    "runtime_admissible": True,
                    "chromaticity": {"x": 0.3, "y": 0.3},
                    "relative_y": {"value": 0.3},
                }
            )
        payload = {
            "schema_version": "1.0",
            "profile_id": f"white/{topology}/warm/r1",
            "runtime_admissible": True,
            "topology": topology,
            "photometric": {"channels": channel_records},
        }
        return admit(parse_artifact(json.dumps(payload).encode("utf-8"), topology))

    def test_a_fully_specified_white_artifact_is_still_refused(
        self: "TestRenderableTopologies",
    ) -> None:
        """No silent reduction to three channels.

        `EmitterProfile` carries three primaries and `EmitterProfile::rgb` is
        its only factory, so rendering an admitted RGBW artifact would emit an
        RGB profile with the white simply gone. That is the quiet data loss
        this gate exists to prevent, so it is refused at admission instead --
        even though every channel here is individually admissible.
        """

        for topology in ("rgbw", "rgbww"):
            with self.subTest(topology=topology):
                decision = self._white_artifact(topology)
                self.assertIsInstance(decision, Refusal)
                assert isinstance(decision, Refusal)
                self.assertTrue(
                    any("no EmitterProfile form yet" in r for r in decision.reasons),
                    msg=f"reasons were {decision.reasons}",
                )

    def test_rgb_is_still_renderable(self: "TestRenderableTopologies") -> None:
        # Guards the screen above from becoming "refuse everything".
        self.assertIsInstance(self._white_artifact("rgb"), Admission)


class TestProvenanceHashing(unittest.TestCase):
    def test_the_hash_is_over_the_bytes_on_disk(self: "TestProvenanceHashing") -> None:
        # A CRLF artifact must hash as its own file does, or the provenance
        # header names a file nobody can reproduce.
        for path in sorted(FIXTURES.glob("*.profile.json")):
            with self.subTest(artifact=path.name):
                artifact = parse_artifact(path.read_bytes(), path.name)
                self.assertEqual(
                    artifact.content_sha256,
                    hashlib.sha256(path.read_bytes()).hexdigest(),
                )

    def test_crlf_changes_the_hash(self: "TestProvenanceHashing") -> None:
        # Non-vacuity for the test above: the two encodings must differ, or it
        # would pass for an implementation that hashed a decoded round trip.
        source = (FIXTURES / "fixture-rgb-none-synthetic-r1.profile.json").read_bytes()
        crlf = source.replace(b"\n", b"\r\n")
        self.assertNotEqual(source, crlf)
        self.assertNotEqual(
            parse_artifact(source, "lf").content_sha256,
            parse_artifact(crlf, "crlf").content_sha256,
        )


class TestMissingDirectory(unittest.TestCase):
    def test_a_missing_artifact_directory_fails_loudly(
        self: "TestMissingDirectory",
    ) -> None:
        # Globbing a missing directory yields nothing, which would let write
        # mode emit a header with those parts -- and their refusal records --
        # simply absent.
        missing = REPO_ROOT / "ci" / "no" / "such" / "directory"
        with self.assertRaises(FileNotFoundError) as caught:
            load_artifacts(missing)
        # The whole path, not a fragment of it. Asserting on a substring like
        # "no" passes for almost any message and would not notice the error
        # naming the wrong directory -- which is the one thing the reader
        # needs it to get right.
        self.assertIn(str(missing), str(caught.exception))


class TestCheckMode(unittest.TestCase):
    def test_check_passes_on_the_checked_in_header(self: "TestCheckMode") -> None:
        self.assertEqual(generate_profile_header.main(["--mode", "check"]), 0)

    def test_check_fails_when_the_header_drifts(self: "TestCheckMode") -> None:
        # Without this the check could be a no-op and nothing would notice.
        original = GENERATED.read_text(encoding="utf-8")
        try:
            GENERATED.write_text(original + "// drift\n", encoding="utf-8")
            self.assertEqual(generate_profile_header.main(["--mode", "check"]), 1)
        finally:
            GENERATED.write_text(original, encoding="utf-8")
        self.assertEqual(generate_profile_header.main(["--mode", "check"]), 0)

    def test_rendering_is_deterministic(self: "TestCheckMode") -> None:
        # A generated file whose bytes move on their own cannot be checked for
        # drift at all -- notably, the date is frozen rather than today's.
        first = generate_profile_header.render([FIXTURES, MIRRORED])
        second = generate_profile_header.render([FIXTURES, MIRRORED])
        self.assertEqual(first, second)


if __name__ == "__main__":
    unittest.main()
