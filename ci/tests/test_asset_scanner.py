"""Unit tests for the v1 asset scanner (FastLED issue #2284)."""

import json
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from ci.compiler.asset_scanner import (
    AssetEntry,
    AssetScanResult,
    _parse_lnk_content,
    manifest_to_cpp_header,
    manifest_to_js_bootstrap,
    scan_sketch_assets,
    write_manifest_json,
)


class TestParseLnkContent(unittest.TestCase):
    """Python-side parsing must match the C++ `parse_lnk_with_metadata`."""

    def test_plain_url(self) -> None:
        entry = _parse_lnk_content("https://example.com/a.mp3\n")
        assert entry is not None
        self.assertEqual(entry.url, "https://example.com/a.mp3")
        self.assertIsNone(entry.sha256)
        self.assertIsNone(entry.fallback)

    def test_comment_and_blank_lines_skipped(self) -> None:
        content = (
            "# this is a comment\n\n  # indented comment\nhttps://example.com/b.mp3\n"
        )
        entry = _parse_lnk_content(content)
        assert entry is not None
        self.assertEqual(entry.url, "https://example.com/b.mp3")

    def test_sha256_and_fallback_metadata(self) -> None:
        content = (
            "https://example.com/c.mp3\n"
            "sha256=deadbeef\n"
            "fallback=https://mirror.example.com/c.mp3\n"
        )
        entry = _parse_lnk_content(content)
        assert entry is not None
        self.assertEqual(entry.sha256, "deadbeef")
        self.assertEqual(entry.fallback, "https://mirror.example.com/c.mp3")

    def test_unknown_metadata_forward_compat(self) -> None:
        content = "https://example.com/d.mp3\ncontent-type=audio/mpeg\n"
        entry = _parse_lnk_content(content)
        assert entry is not None
        # Unknown key must NOT raise and must NOT populate anything.
        self.assertIsNone(entry.sha256)
        self.assertIsNone(entry.fallback)

    def test_empty_content_returns_none(self) -> None:
        self.assertIsNone(_parse_lnk_content(""))
        self.assertIsNone(_parse_lnk_content("# comment only\n"))


class TestScanSketchAssets(unittest.TestCase):
    """End-to-end: scan a temp sketch directory with a .lnk inside data/."""

    def test_scan_missing_data_dir_is_not_an_error(self) -> None:
        with TemporaryDirectory() as tmp:
            result = scan_sketch_assets(Path(tmp))
            self.assertEqual(result.manifest, {})
            self.assertEqual(result.warnings, [])

    def test_scan_picks_up_lnk_files(self) -> None:
        with TemporaryDirectory() as tmp:
            sketch = Path(tmp)
            data = sketch / "data"
            data.mkdir()
            (data / "track.mp3.lnk").write_text(
                "# comment\nhttps://example.com/track.mp3\nsha256=abc\n",
                encoding="utf-8",
            )
            (data / "voice.wav.lnk").write_text(
                "https://example.com/voice.wav\n", encoding="utf-8"
            )
            # Non-lnk files must be ignored.
            (data / "readme.txt").write_text("ignore me", encoding="utf-8")

            result = scan_sketch_assets(sketch)
            self.assertEqual(
                set(result.manifest.keys()), {"data/track.mp3", "data/voice.wav"}
            )
            self.assertEqual(
                result.manifest["data/track.mp3"].url,
                "https://example.com/track.mp3",
            )
            self.assertEqual(result.manifest["data/track.mp3"].sha256, "abc")
            self.assertEqual(result.warnings, [])

    def test_malformed_lnk_produces_warning_not_exception(self) -> None:
        with TemporaryDirectory() as tmp:
            sketch = Path(tmp)
            data = sketch / "data"
            data.mkdir()
            # An empty .lnk file has no URL.
            (data / "broken.mp3.lnk").write_text("# comment only\n", encoding="utf-8")

            result = scan_sketch_assets(sketch)
            self.assertEqual(result.manifest, {})
            self.assertEqual(len(result.warnings), 1)
            self.assertIn("no URL found", result.warnings[0])


class TestManifestSerializers(unittest.TestCase):
    def test_write_manifest_json_roundtrip(self) -> None:
        scan = AssetScanResult(
            manifest={
                "data/a.mp3": AssetEntry(url="https://example.com/a.mp3"),
            }
        )
        with TemporaryDirectory() as tmp:
            out = Path(tmp) / "asset_manifest.json"
            write_manifest_json(scan, out)
            on_disk = json.loads(out.read_text(encoding="utf-8"))
            self.assertEqual(
                on_disk,
                {
                    "data/a.mp3": {
                        "url": "https://example.com/a.mp3",
                        "sha256": None,
                        "fallback": None,
                    }
                },
            )

    def test_js_bootstrap_is_valid_assignment(self) -> None:
        scan = AssetScanResult(
            manifest={"data/a.mp3": AssetEntry(url="https://example.com/a.mp3")}
        )
        js = manifest_to_js_bootstrap(scan)
        self.assertIn("window.fastledAssetManifest", js)
        self.assertIn("https://example.com/a.mp3", js)

    def test_cpp_header_uses_register_asset(self) -> None:
        scan = AssetScanResult(
            manifest={"data/track.mp3": AssetEntry(url="https://example.com/a.mp3")}
        )
        header = manifest_to_cpp_header(scan)
        self.assertIn('#include "fl/asset/asset.h"', header)
        self.assertIn("::fl::register_asset", header)
        self.assertIn('"data/track.mp3"', header)
        self.assertIn('"https://example.com/a.mp3"', header)

    def test_cpp_header_escapes_quotes_in_path(self) -> None:
        scan = AssetScanResult(
            manifest={'data/"weird".mp3': AssetEntry(url='https://x/"y".mp3')}
        )
        header = manifest_to_cpp_header(scan)
        self.assertIn('\\"weird\\"', header)
        self.assertIn('\\"y\\"', header)


class TestParseLnkJsonFormat(unittest.TestCase):
    """fbuild's JSON `.lnk` schema must parse through the same entry point."""

    def test_parses_fbuild_json_lnk(self) -> None:
        entry = _parse_lnk_content(
            json.dumps(
                {
                    "v": 1,
                    "url": "https://cdn.example/video1.rgb",
                    "sha256": "a" * 64,
                    "size": 6051840,
                }
            )
        )
        assert entry is not None
        self.assertEqual(entry.url, "https://cdn.example/video1.rgb")
        self.assertEqual(entry.sha256, "a" * 64)
        self.assertEqual(entry.size, 6051840)

    def test_json_url_list_makes_second_entry_the_fallback(self) -> None:
        entry = _parse_lnk_content(
            json.dumps(
                {"v": 1, "url": ["https://cdn.example/a", "https://raw.example/a"]}
            )
        )
        assert entry is not None
        self.assertEqual(entry.url, "https://cdn.example/a")
        self.assertEqual(entry.fallback, "https://raw.example/a")

    def test_json_extract_field_is_captured(self) -> None:
        entry = _parse_lnk_content(
            json.dumps({"v": 1, "url": "https://x/a.zip", "extract": "zip"})
        )
        assert entry is not None
        self.assertEqual(entry.extract, "zip")

    def test_json_without_url_is_rejected(self) -> None:
        self.assertIsNone(_parse_lnk_content(json.dumps({"v": 1, "sha256": "a" * 64})))

    def test_text_format_still_wins_when_not_json(self) -> None:
        """A leading comment must not push the sniffer into the JSON branch."""
        lines = ["# a comment", "https://example.com/a.mp3", "sha256=abc", ""]
        entry = _parse_lnk_content("\n".join(lines))
        assert entry is not None
        self.assertEqual(entry.url, "https://example.com/a.mp3")
        self.assertEqual(entry.sha256, "abc")
        self.assertIsNone(entry.size)

    def test_text_entry_serializes_without_the_new_fields(self) -> None:
        """Existing manifest consumers must see a byte-identical shape."""
        scan = AssetScanResult(
            manifest={"data/a.mp3": AssetEntry(url="https://x/a.mp3")}
        )
        self.assertEqual(
            scan.to_json_dict(),
            {
                "data/a.mp3": {
                    "url": "https://x/a.mp3",
                    "sha256": None,
                    "fallback": None,
                }
            },
        )


class TestAssetsJsonManifest(unittest.TestCase):
    """A sketch may declare many assets in one `assets.json`."""

    def _sketch(self, tmp: str, files: dict[str, str]) -> Path:
        sketch = Path(tmp) / "Sketch"
        (sketch / "data").mkdir(parents=True)
        for name, body in files.items():
            (sketch / "data" / name).write_text(body, encoding="utf-8")
        return sketch

    def test_assets_json_entries_are_scanned(self) -> None:
        with TemporaryDirectory() as tmp:
            sketch = self._sketch(
                tmp,
                {
                    "assets.json": json.dumps(
                        {
                            "assets": {
                                "video.rgb": {"url": "https://x/video.rgb", "size": 42}
                            }
                        }
                    )
                },
            )
            scan = scan_sketch_assets(sketch)
            self.assertIn("data/video.rgb", scan.manifest)
            self.assertEqual(scan.manifest["data/video.rgb"].size, 42)

    def test_lnk_overrides_assets_json_for_the_same_name(self) -> None:
        with TemporaryDirectory() as tmp:
            sketch = self._sketch(
                tmp,
                {
                    "assets.json": json.dumps(
                        {"assets": {"dup.bin": {"url": "https://json.example/dup.bin"}}}
                    ),
                    "dup.bin.lnk": "https://lnk.example/dup.bin\n",
                },
            )
            scan = scan_sketch_assets(sketch)
            self.assertEqual(
                scan.manifest["data/dup.bin"].url, "https://lnk.example/dup.bin"
            )

    def test_both_formats_coexist_in_one_sketch(self) -> None:
        with TemporaryDirectory() as tmp:
            sketch = self._sketch(
                tmp,
                {
                    "assets.json": json.dumps(
                        {
                            "assets": {
                                "from_json.bin": {"url": "https://x/from_json.bin"}
                            }
                        }
                    ),
                    "from_text.bin.lnk": "https://x/from_text.bin\n",
                    "from_json_lnk.bin.lnk": json.dumps(
                        {"v": 1, "url": "https://x/from_json_lnk.bin"}
                    ),
                },
            )
            scan = scan_sketch_assets(sketch)
            self.assertEqual(
                sorted(scan.manifest),
                ["data/from_json.bin", "data/from_json_lnk.bin", "data/from_text.bin"],
            )
            self.assertEqual(scan.warnings, [])

    def test_malformed_assets_json_warns_without_failing(self) -> None:
        with TemporaryDirectory() as tmp:
            sketch = self._sketch(tmp, {"assets.json": "{not json"})
            scan = scan_sketch_assets(sketch)
            self.assertEqual(scan.manifest, {})
            self.assertTrue(any("invalid JSON" in w for w in scan.warnings))

    def test_one_bad_entry_does_not_discard_the_rest(self) -> None:
        with TemporaryDirectory() as tmp:
            sketch = self._sketch(
                tmp,
                {
                    "assets.json": json.dumps(
                        {
                            "assets": {
                                "good.bin": {"url": "https://x/good"},
                                "bad.bin": {},
                            }
                        }
                    )
                },
            )
            scan = scan_sketch_assets(sketch)
            self.assertIn("data/good.bin", scan.manifest)
            self.assertNotIn("data/bad.bin", scan.manifest)
            self.assertTrue(any("bad.bin" in w for w in scan.warnings))


class TestRealCommittedLnkFiles(unittest.TestCase):
    """The `.lnk` file committed in this repo must parse."""

    def test_audiourl_text_lnk_parses(self) -> None:
        lnk = Path("examples/AudioUrl/data/track.mp3.lnk")
        if not lnk.is_file():
            self.skipTest("example not present")
        entry = _parse_lnk_content(lnk.read_text(encoding="utf-8"))
        assert entry is not None
        self.assertTrue(entry.url.startswith("https://"))


if __name__ == "__main__":
    unittest.main()
