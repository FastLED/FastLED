"""Tests for peer-OTA status/port hardening (FastLED#3956).

The peer-OTA flow proves the RP2350W downloaded its image by asserting the
ESP32-C6 reports `servedRequests >= 1`. Both halves of that check were
fragile: a non-numeric `servedRequests` raised out of the handler as a
traceback, and the artifact-server port was accepted without a range check
even though the device narrows it to a uint16.
"""

from __future__ import annotations

import unittest
from pathlib import Path
from typing import Any

from ci.autoresearch.ota import _served_request_count


class TestServedRequestCount(unittest.TestCase):
    """`servedRequests` arrives from the device and cannot be trusted."""

    def test_plain_counts_pass_through(self) -> None:
        self.assertEqual(_served_request_count({"servedRequests": 1}), 1)
        self.assertEqual(_served_request_count({"servedRequests": 7}), 7)
        self.assertEqual(_served_request_count({"servedRequests": 0}), 0)

    def test_missing_key_is_zero(self) -> None:
        self.assertEqual(_served_request_count({}), 0)

    def test_non_numeric_values_do_not_raise(self) -> None:
        """The regression: these used to escape as ValueError/TypeError.

        Neither is in the caller's `except` tuple, so the run aborted with a
        traceback instead of the normal failure message.
        """
        values: list[object] = ["many", "", None, [], {}, object()]
        for value in values:
            status: dict[str, Any] = {"servedRequests": value}
            self.assertEqual(
                _served_request_count(status),
                0,
                f"{value!r} must read as zero, not raise",
            )

    def test_float_is_not_accepted_as_a_count(self) -> None:
        # A float would have silently truncated through int().
        self.assertEqual(_served_request_count({"servedRequests": 1.9}), 0)

    def test_bool_does_not_count_as_a_served_request(self) -> None:
        # bool is an int subclass; True must not satisfy ">= 1".
        self.assertEqual(_served_request_count({"servedRequests": True}), 0)
        self.assertEqual(_served_request_count({"servedRequests": False}), 0)

    def test_negative_counts_clamp_to_zero(self) -> None:
        self.assertEqual(_served_request_count({"servedRequests": -5}), 0)

    def test_a_failed_transfer_cannot_satisfy_the_proof(self) -> None:
        """A 404 or partial send must not read as a served transfer."""
        values: list[object] = [0, None, "0", False, -1]
        for value in values:
            self.assertLess(
                _served_request_count({"servedRequests": value}),
                1,
                f"{value!r} must not prove the artifact was served",
            )


class TestArtifactServerPortValidation(unittest.TestCase):
    """Mirrors the guard applied to `startOtaArtifactServer`'s reply.

    The device narrows this port to a uint16, so a value outside that range
    silently retargets the update rather than failing.
    """

    @staticmethod
    def _accepted(host: object, port: object) -> bool:
        return (
            isinstance(host, str)
            and isinstance(port, int)
            and not isinstance(port, bool)
            and 1 <= port <= 65535
        )

    def test_valid_ports_accepted(self) -> None:
        self.assertTrue(self._accepted("192.168.4.1", 8081))
        self.assertTrue(self._accepted("192.168.4.1", 1))
        self.assertTrue(self._accepted("192.168.4.1", 65535))

    def test_out_of_range_ports_rejected(self) -> None:
        for port in (0, -1, 65536, 65537, 70000, 74001):
            self.assertFalse(
                self._accepted("192.168.4.1", port),
                f"port {port} must be rejected before narrowing",
            )

    def test_non_int_and_bool_ports_rejected(self) -> None:
        ports: list[object] = ["8081", None, 8081.0, True, False, []]
        for port in ports:
            self.assertFalse(self._accepted("192.168.4.1", port), f"{port!r}")

    def test_non_string_host_rejected(self) -> None:
        self.assertFalse(self._accepted(None, 8081))
        self.assertFalse(self._accepted(1234, 8081))


OTA_IMPL = (
    Path(__file__).resolve().parents[2] / "examples/AutoResearch/AutoResearchOta.cpp"
)
NET_RPC = (
    Path(__file__).resolve().parents[2]
    / "examples/AutoResearch/AutoResearchRemoteNetworkMethods.cpp"
)


class TestServeOtaArtifactFraming(unittest.TestCase):
    """Device-side invariants for the C6 artifact handler.

    The handler is compile-gated to FL_IS_ESP32, so these pin the source
    contract that the host-side proof in ota.py depends on.
    """

    def test_served_counter_increments_after_the_transfer(self) -> None:
        source = OTA_IMPL.read_text(encoding="utf-8")
        start = source.index("static esp_err_t serveOtaArtifact(")
        body = source[start : source.index("\n}", start)]

        open_at = body.index("LittleFS.open")
        bump_at = body.index("++getOtaArtifactState().served_requests")
        self.assertGreater(
            bump_at,
            open_at,
            "a 404 must not count as a served request",
        )
        terminator_at = body.rindex("httpd_resp_send_chunk(request, nullptr, 0)")
        self.assertGreater(
            bump_at,
            terminator_at,
            "the counter must bump only after the final chunk is accepted",
        )

    def test_chunked_response_carries_no_content_length(self) -> None:
        source = OTA_IMPL.read_text(encoding="utf-8")
        start = source.index("static esp_err_t serveOtaArtifact(")
        body = source[start : source.index("\n}", start)]

        self.assertIn("httpd_resp_send_chunk", body)
        # Look for the header call itself — prose mentioning Content-Length
        # in a comment is fine, emitting the header is not.
        self.assertNotIn(
            'httpd_resp_set_hdr(request, "Content-Length"',
            body,
            "chunked transfer-encoding and Content-Length are conflicting framings",
        )


class TestRpcPortValidation(unittest.TestCase):
    def test_port_rpcs_use_the_range_checked_parser(self) -> None:
        source = NET_RPC.read_text(encoding="utf-8")
        self.assertIn("fl/net/port.h", source)
        # Both call sites must go through the checked parser rather than
        # narrowing directly.
        self.assertEqual(source.count("fl::net::tryParsePort"), 2)
        self.assertNotIn("static_cast<uint16_t>(port)", source)
        self.assertNotIn("static_cast<uint16_t>(port_val.as_int().value())", source)


class TestOtaWatchdogHandling(unittest.TestCase):
    def test_blocking_update_widens_and_restores_the_window(self) -> None:
        source = OTA_IMPL.read_text(encoding="utf-8")
        start = source.index("void pollOtaArtifactUpdate(uint32_t watchdog_restore_ms)")
        body = source[start : source.index("\n}", start)]

        widen_at = body.index("wdt.begin(kOtaUpdateWatchdogTimeoutMs)")
        # The call, not the comment that explains why it is wrapped.
        update_at = body.index("result = updater.update(")
        restore_at = body.index("wdt.begin(watchdog_restore_ms)")

        self.assertLess(widen_at, update_at, "widen before the blocking download")
        self.assertLess(update_at, restore_at, "restore after it returns")


if __name__ == "__main__":
    unittest.main()
