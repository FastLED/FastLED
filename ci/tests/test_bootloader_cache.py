"""Tests for ``ci.compiler.bootloader_cache``.

Covers:
- deterministic cache-key hashing across content-equivalent inputs
- cache-key divergence for any input change
- miss → populate → hit round-trip
- partially populated entries are treated as a miss
- ``materialise_into`` deposits both files
- manifest content is JSON and records all four key inputs
"""

from __future__ import annotations

import json
import shutil
import tempfile
from pathlib import Path
from unittest import TestCase

from ci.compiler.bootloader_cache import (
    BootloaderCache,
    BootloaderCacheKey,
    build_cache_key_for_board,
)


class TestBootloaderCacheKey(TestCase):
    """Tests for the cache-key hashing contract."""

    def test_same_inputs_same_digest(self) -> None:
        k1 = BootloaderCacheKey(
            board="esp32dev",
            idf_version="https://example.com/platform.zip",
            partition_csv_content="# header\nfoo,app,ota_0,0x10000,0x140000\n",
            sdkconfig_defaults_content="CONFIG_FOO=y\n",
        )
        k2 = BootloaderCacheKey(
            board="esp32dev",
            idf_version="https://example.com/platform.zip",
            partition_csv_content="# header\nfoo,app,ota_0,0x10000,0x140000\n",
            sdkconfig_defaults_content="CONFIG_FOO=y\n",
        )
        self.assertEqual(k1.digest(), k2.digest())

    def test_board_changes_digest(self) -> None:
        base = dict(
            idf_version="v1",
            partition_csv_content="csv",
            sdkconfig_defaults_content="sdk",
        )
        a = BootloaderCacheKey(board="esp32dev", **base).digest()
        b = BootloaderCacheKey(board="esp32c3", **base).digest()
        self.assertNotEqual(a, b)

    def test_idf_version_changes_digest(self) -> None:
        base = dict(
            board="esp32dev",
            partition_csv_content="csv",
            sdkconfig_defaults_content="sdk",
        )
        a = BootloaderCacheKey(idf_version="v1", **base).digest()
        b = BootloaderCacheKey(idf_version="v2", **base).digest()
        self.assertNotEqual(a, b)

    def test_partition_csv_changes_digest(self) -> None:
        base = dict(
            board="esp32dev",
            idf_version="v1",
            sdkconfig_defaults_content="sdk",
        )
        a = BootloaderCacheKey(partition_csv_content="csv-A", **base).digest()
        b = BootloaderCacheKey(partition_csv_content="csv-B", **base).digest()
        self.assertNotEqual(a, b)

    def test_sdkconfig_changes_digest(self) -> None:
        base = dict(
            board="esp32dev",
            idf_version="v1",
            partition_csv_content="csv",
        )
        a = BootloaderCacheKey(sdkconfig_defaults_content="CONFIG_A=y", **base).digest()
        b = BootloaderCacheKey(sdkconfig_defaults_content="CONFIG_B=y", **base).digest()
        self.assertNotEqual(a, b)

    def test_crlf_and_lf_equivalent(self) -> None:
        # Windows checkouts use CRLF; Linux uses LF. The same logical
        # content must produce the same digest regardless of EOLs.
        a = BootloaderCacheKey(
            board="esp32dev",
            idf_version="v1",
            partition_csv_content="foo\r\nbar\r\n",
            sdkconfig_defaults_content="",
        ).digest()
        b = BootloaderCacheKey(
            board="esp32dev",
            idf_version="v1",
            partition_csv_content="foo\nbar\n",
            sdkconfig_defaults_content="",
        ).digest()
        self.assertEqual(a, b)

    def test_digest_is_sha256_hex_length(self) -> None:
        digest = BootloaderCacheKey(
            board="esp32dev",
            idf_version="v1",
            partition_csv_content="",
            sdkconfig_defaults_content="",
        ).digest()
        self.assertEqual(len(digest), 64)
        int(digest, 16)  # must parse as hex


class TestBootloaderCache(TestCase):
    """Tests for the on-disk cache behaviour."""

    def setUp(self) -> None:
        self.tmp = Path(tempfile.mkdtemp())
        self.cache = BootloaderCache(cache_dir=self.tmp / "cache")
        self.key = BootloaderCacheKey(
            board="esp32dev",
            idf_version="https://example.com/platform.zip",
            partition_csv_content="csv-body",
            sdkconfig_defaults_content="CONFIG_A=y",
        )
        # Fake build outputs.
        self.src_dir = self.tmp / "src"
        self.src_dir.mkdir()
        self.src_bootloader = self.src_dir / "bootloader.bin"
        self.src_partitions = self.src_dir / "partitions.bin"
        self.src_bootloader.write_bytes(b"BOOTLOADER-BYTES")
        self.src_partitions.write_bytes(b"PARTITIONS-BYTES")

    def tearDown(self) -> None:
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_miss_then_populate_then_hit(self) -> None:
        self.assertIsNone(self.cache.lookup(self.key))
        populated = self.cache.populate(
            self.key, self.src_bootloader, self.src_partitions
        )
        # Entry dir should live under cache_dir/<digest>/
        self.assertEqual(populated.entry_dir.parent, self.cache.cache_dir)
        self.assertEqual(populated.entry_dir.name, self.key.digest())

        hit = self.cache.lookup(self.key)
        self.assertIsNotNone(hit)
        assert hit is not None  # for type narrowing
        self.assertEqual(hit.bootloader_bin.read_bytes(), b"BOOTLOADER-BYTES")
        self.assertEqual(hit.partitions_bin.read_bytes(), b"PARTITIONS-BYTES")

    def test_missing_breadcrumb_is_a_miss(self) -> None:
        populated = self.cache.populate(
            self.key, self.src_bootloader, self.src_partitions
        )
        # Remove breadcrumb to simulate a half-written entry.
        (populated.entry_dir / ".populated").unlink()
        self.assertIsNone(self.cache.lookup(self.key))

    def test_deleted_artifact_after_breadcrumb_is_a_miss(self) -> None:
        populated = self.cache.populate(
            self.key, self.src_bootloader, self.src_partitions
        )
        # Remove a real file but leave the breadcrumb behind — simulates
        # out-of-band deletion (disk cleanup, bad actor). Lookup must
        # treat this as a miss, not a hit that returns a broken path.
        populated.bootloader_bin.unlink()
        self.assertIsNone(self.cache.lookup(self.key))

    def test_manifest_records_inputs(self) -> None:
        populated = self.cache.populate(
            self.key, self.src_bootloader, self.src_partitions
        )
        manifest = json.loads(populated.manifest.read_text(encoding="utf-8"))
        self.assertEqual(manifest["cache_key"], self.key.digest())
        self.assertEqual(manifest["inputs"]["board"], "esp32dev")
        self.assertEqual(
            manifest["inputs"]["idf_version"],
            "https://example.com/platform.zip",
        )
        self.assertIn("partition_csv_sha256", manifest["inputs"])
        self.assertIn("sdkconfig_defaults_sha256", manifest["inputs"])
        self.assertEqual(manifest["bootloader_size"], len(b"BOOTLOADER-BYTES"))
        self.assertEqual(manifest["partitions_size"], len(b"PARTITIONS-BYTES"))

    def test_populate_rejects_missing_source(self) -> None:
        with self.assertRaises(FileNotFoundError):
            self.cache.populate(
                self.key, self.src_dir / "does-not-exist.bin", self.src_partitions
            )

    def test_materialise_into_deposits_both_files(self) -> None:
        populated = self.cache.populate(
            self.key, self.src_bootloader, self.src_partitions
        )
        dest = self.tmp / "release"
        self.assertFalse(dest.exists())

        hit = self.cache.lookup(self.key)
        assert hit is not None

        bootloader_dst, partitions_dst = self.cache.materialise_into(hit, dest)
        self.assertTrue(bootloader_dst.exists())
        self.assertTrue(partitions_dst.exists())
        self.assertEqual(bootloader_dst.read_bytes(), b"BOOTLOADER-BYTES")
        self.assertEqual(partitions_dst.read_bytes(), b"PARTITIONS-BYTES")

    def test_repopulating_same_key_is_idempotent(self) -> None:
        # Populate twice with the same key (e.g. two parallel CI jobs):
        # the second call should still yield a valid cached entry.
        self.cache.populate(self.key, self.src_bootloader, self.src_partitions)
        self.cache.populate(self.key, self.src_bootloader, self.src_partitions)
        hit = self.cache.lookup(self.key)
        self.assertIsNotNone(hit)


class TestBuildCacheKeyForBoard(TestCase):
    """Tests for the convenience key-builder that reads a build dir."""

    def setUp(self) -> None:
        self.tmp = Path(tempfile.mkdtemp())

    def tearDown(self) -> None:
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_reads_partitions_csv_and_sdkconfig_from_build_dir(self) -> None:
        (self.tmp / "partitions.csv").write_text("csv-content\n", encoding="utf-8")
        (self.tmp / "sdkconfig.defaults").write_text("CONFIG_X=y\n", encoding="utf-8")
        key = build_cache_key_for_board(
            board_name="esp32dev",
            platform_url_or_version="v1",
            build_dir=self.tmp,
        )
        self.assertEqual(key.board, "esp32dev")
        self.assertEqual(key.idf_version, "v1")
        self.assertEqual(key.partition_csv_content, "csv-content\n")
        self.assertEqual(key.sdkconfig_defaults_content, "CONFIG_X=y\n")

    def test_missing_files_produce_empty_content(self) -> None:
        # Build dir exists but contains no partitions.csv / sdkconfig.defaults
        # — e.g. boards that rely solely on PIO-managed defaults.
        key = build_cache_key_for_board(
            board_name="esp32dev",
            platform_url_or_version="v1",
            build_dir=self.tmp,
        )
        self.assertEqual(key.partition_csv_content, "")
        self.assertEqual(key.sdkconfig_defaults_content, "")
        # Still produces a valid digest.
        self.assertEqual(len(key.digest()), 64)

    def test_overrides_bypass_file_reads(self) -> None:
        # Build dir is empty but we pass explicit overrides — they win.
        key = build_cache_key_for_board(
            board_name="esp32dev",
            platform_url_or_version="v1",
            build_dir=self.tmp,
            partition_csv_override="OVERRIDE-CSV",
            sdkconfig_defaults_override="OVERRIDE-SDK",
        )
        self.assertEqual(key.partition_csv_content, "OVERRIDE-CSV")
        self.assertEqual(key.sdkconfig_defaults_content, "OVERRIDE-SDK")


class TestCacheLifecycleSimulation(TestCase):
    """End-to-end simulation of the cold-cache → warm-cache lifecycle.

    Exercises the same flow the real ``_apply_bootloader_cache`` hook in
    ``build_with_merged_bin`` uses, without requiring a real ESP-IDF
    build to run. This is the unit-level analogue of the cold/warm CI
    assertions the PR body promises.
    """

    def setUp(self) -> None:
        self.tmp = Path(tempfile.mkdtemp())
        self.cache_root = self.tmp / "cache"
        # Backend output dir (acts like .pio/build/<env>/ or
        # .fbuild/build/<env>/release/).
        self.backend_dir = self.tmp / "backend"
        self.backend_dir.mkdir()

    def tearDown(self) -> None:
        shutil.rmtree(self.tmp, ignore_errors=True)

    def _simulate_build(self, produce_boot_artifacts: bool) -> None:
        """Drop fake firmware.bin and optionally boot artifacts."""
        (self.backend_dir / "firmware.bin").write_bytes(b"FW")
        if produce_boot_artifacts:
            (self.backend_dir / "bootloader.bin").write_bytes(b"BOOT-V1")
            (self.backend_dir / "partitions.bin").write_bytes(b"PART-V1")

    def test_cold_populate_then_warm_hit_materialises(self) -> None:
        cache = BootloaderCache(cache_dir=self.cache_root)
        key = BootloaderCacheKey(
            board="esp32dev",
            idf_version="pioarduino/54.03.20",
            partition_csv_content="csv",
            sdkconfig_defaults_content="",
        )

        # --- Cold run: backend produces boot artifacts → we populate.
        self._simulate_build(produce_boot_artifacts=True)
        self.assertIsNone(cache.lookup(key))
        cache.populate(
            key,
            self.backend_dir / "bootloader.bin",
            self.backend_dir / "partitions.bin",
        )
        hit = cache.lookup(key)
        self.assertIsNotNone(hit)

        # --- Warm run: simulate a backend that did NOT emit boot
        # artifacts (older fbuild, or they got deleted). The cache
        # rescues us by materialising the cached files into the
        # backend's dir.
        shutil.rmtree(self.backend_dir)
        self.backend_dir.mkdir()
        self._simulate_build(produce_boot_artifacts=False)
        self.assertFalse((self.backend_dir / "bootloader.bin").exists())

        hit = cache.lookup(key)
        assert hit is not None
        cache.materialise_into(hit, self.backend_dir)

        self.assertTrue((self.backend_dir / "bootloader.bin").exists())
        self.assertTrue((self.backend_dir / "partitions.bin").exists())
        self.assertEqual((self.backend_dir / "bootloader.bin").read_bytes(), b"BOOT-V1")
        self.assertEqual((self.backend_dir / "partitions.bin").read_bytes(), b"PART-V1")

    def test_partition_csv_change_invalidates_cache(self) -> None:
        cache = BootloaderCache(cache_dir=self.cache_root)
        key_v1 = BootloaderCacheKey(
            board="esp32dev",
            idf_version="pioarduino/54.03.20",
            partition_csv_content="csv-A",
            sdkconfig_defaults_content="",
        )
        self._simulate_build(produce_boot_artifacts=True)
        cache.populate(
            key_v1,
            self.backend_dir / "bootloader.bin",
            self.backend_dir / "partitions.bin",
        )

        # Now the user tweaks partitions.csv — cache must miss so the
        # next build regenerates artifacts with the new layout.
        key_v2 = BootloaderCacheKey(
            board="esp32dev",
            idf_version="pioarduino/54.03.20",
            partition_csv_content="csv-B (user edited)",
            sdkconfig_defaults_content="",
        )
        self.assertIsNone(cache.lookup(key_v2))
        # v1 still hits for users who haven't upgraded.
        self.assertIsNotNone(cache.lookup(key_v1))
