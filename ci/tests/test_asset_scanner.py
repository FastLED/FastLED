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


if __name__ == "__main__":
    unittest.main()
