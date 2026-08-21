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

    @staticmethod
    def _body() -> str:
        source = OTA_IMPL.read_text(encoding="utf-8")
        start = source.index("static esp_err_t serveOtaArtifact(")
        # The handler's closing brace is the first one at column 0.
        end = source.index("\n}", start)
        return source[start:end]

    def test_served_counter_increments_after_the_transfer(self) -> None:
        body = self._body()
        open_at = body.index("LittleFS.open")
        bump_at = body.index("++getOtaArtifactState().served_requests")
        self.assertGreater(bump_at, open_at, "a 404 must not count as a served request")
        terminator_at = body.rindex("httpd_resp_send_chunk(request, nullptr, 0)")
        self.assertGreater(
            bump_at,
            terminator_at,
            "the counter must bump only after the final chunk is accepted",
        )

    def test_response_reports_content_length_for_the_ota_client(self) -> None:
        """The pinned arduino-pico HTTPUpdate requires Content-Length.

        HTTPClient::_size starts at -1 and is only assigned from a
        Content-Length header; HTTPUpdate fails with
        HTTP_UE_SERVER_NOT_REPORT_SIZE unless that size is > 0. Dropping the
        header would make every peer OTA fail, so it must stay even though
        the body streams out chunked.
        """
        body = self._body()
        self.assertIn("httpd_resp_send_chunk", body)
        self.assertIn('httpd_resp_set_hdr(request, "Content-Length"', body)

    def test_failed_send_does_not_terminate_the_body(self) -> None:
        """A zero-length chunk means 'body ended normally'.

        Emitting one after a failed read would present a half-written image
        as a complete one.
        """
        body = self._body()
        failure_branch = body[
            body.index("if (read == 0") : body.index("return ESP_FAIL;")
        ]
        self.assertNotIn("httpd_resp_send_chunk(request, nullptr, 0)", failure_branch)


class TestRpcPortValidation(unittest.TestCase):
    def test_port_rpcs_use_the_range_checked_parser(self) -> None:
        source = NET_RPC.read_text(encoding="utf-8")
        self.assertIn("fl/net/port.h", source)
        # Both call sites must go through the checked parser rather than
        # narrowing directly.
        # At least the two OTA/net RPCs; a future third call site is fine.
        self.assertGreaterEqual(source.count("fl::net::tryParsePort"), 2)
        self.assertNotIn("static_cast<uint16_t>(port)", source)
        self.assertNotIn("static_cast<uint16_t>(port_val.as_int().value())", source)


class TestOtaWatchdogHandling(unittest.TestCase):
    def test_blocking_update_halts_and_rearms_the_watchdog(self) -> None:
        """begin() clamps to ~16.8 s on RP2350, so a longer timeout is a no-op.

        The counter has to be halted across the download and re-armed after.
        """
        source = OTA_IMPL.read_text(encoding="utf-8")
        start = source.index("void pollOtaArtifactUpdate(uint32_t watchdog_restore_ms)")
        body = source[start : source.index("\n}", start)]
        # Collapse whitespace so clang-format line wrapping cannot break these.
        flat = " ".join(body.split())

        disable_at = flat.index("wdt.disable();")
        update_at = flat.index("result = updater.update(")
        rearm_at = flat.index("wdt.begin(watchdog_restore_ms);")

        self.assertLess(disable_at, update_at, "halt before the blocking download")
        self.assertLess(update_at, rearm_at, "re-arm after it returns")
        # Requesting a long timeout instead would be silently clamped.
        self.assertNotIn("kOtaUpdateWatchdogTimeoutMs", flat)


if __name__ == "__main__":
    unittest.main()
