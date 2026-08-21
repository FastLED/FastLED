"""Tests for BLE notification reassembly (FastLED#3955).

The RP2350W transport chunks each JSON-RPC response across ATT-MTU-sized
notifications. The reader must reassemble those fragments, deliver each
complete value independently, and stay usable after a malformed value.
"""

from __future__ import annotations

import json
import unittest
from typing import Any

from ci.util.ble_notify_assembler import (
    REMOTE_PREFIX,
    BleNotificationAssembler,
)


def _pong(ident: int = 1) -> str:
    return REMOTE_PREFIX + json.dumps(
        {
            "jsonrpc": "2.0",
            "id": ident,
            "result": {"message": "pong", "transport": "ble", "uptimeMs": 1234},
        },
        separators=(",", ":"),
    )


def _chunks(text: str, size: int) -> list[str]:
    return [text[i : i + size] for i in range(0, len(text), size)]


class TestBleNotificationAssembler(unittest.TestCase):
    def test_single_notification_parses(self) -> None:
        asm = BleNotificationAssembler()
        values = asm.push(_pong())
        self.assertEqual(len(values), 1)
        self.assertEqual(values[0]["result"]["message"], "pong")

    def test_reassembles_mtu_fragments(self) -> None:
        """One response split across many notifications yields one value."""
        asm = BleNotificationAssembler()
        message = _pong()
        collected: list[Any] = []
        fragments = _chunks(message, 20)
        self.assertGreater(len(fragments), 1, "test needs a fragmented message")
        for fragment in fragments:
            collected.extend(asm.push(fragment))
        self.assertEqual(len(collected), 1)
        self.assertEqual(collected[0]["result"]["message"], "pong")

    def test_two_responses_in_one_stream_are_independent(self) -> None:
        """A second complete value must not be appended to the first."""
        asm = BleNotificationAssembler()
        first = asm.push(_pong(1))
        second = asm.push(_pong(2))
        self.assertEqual(len(first), 1)
        self.assertEqual(len(second), 1)
        self.assertEqual(first[0]["id"], 1)
        self.assertEqual(second[0]["id"], 2)

    def test_two_responses_in_a_single_fragment(self) -> None:
        asm = BleNotificationAssembler()
        values = asm.push(_pong(1) + _pong(2))
        self.assertEqual([v["id"] for v in values], [1, 2])

    def test_valid_pong_after_malformed_value(self) -> None:
        """The regression: garbage must not poison the buffer forever."""
        asm = BleNotificationAssembler()
        self.assertEqual(asm.push(REMOTE_PREFIX + "{not json"), [])
        values = asm.push(_pong(7))
        self.assertEqual(len(values), 1, "valid pong must survive a bad predecessor")
        self.assertEqual(values[0]["id"], 7)

    def test_valid_pong_after_unrelated_complete_value(self) -> None:
        """A complete non-matching value must not absorb the next one."""
        asm = BleNotificationAssembler()
        noise = asm.push(REMOTE_PREFIX + '{"jsonrpc":"2.0","id":0,"result":{}}')
        self.assertEqual(len(noise), 1)
        values = asm.push(_pong(2))
        self.assertEqual(len(values), 1)
        self.assertEqual(values[0]["result"]["message"], "pong")

    def test_partial_value_is_held_not_dropped(self) -> None:
        asm = BleNotificationAssembler()
        message = _pong(3)
        head, tail = message[:25], message[25:]
        self.assertEqual(asm.push(head), [])
        self.assertNotEqual(asm.buffered, "")
        values = asm.push(tail)
        self.assertEqual(len(values), 1)
        self.assertEqual(asm.buffered, "")

    def test_values_without_prefix_still_parse(self) -> None:
        """The READ-characteristic fallback has no REMOTE: marker."""
        asm = BleNotificationAssembler()
        values = asm.push('{"jsonrpc":"2.0","id":9,"result":{"message":"pong"}}')
        self.assertEqual(len(values), 1)
        self.assertEqual(values[0]["id"], 9)

    def test_whitespace_and_newline_delimiters_are_tolerated(self) -> None:
        asm = BleNotificationAssembler()
        values = asm.push("\n" + _pong(4) + "\n")
        self.assertEqual(len(values), 1)
        values = asm.push("  " + _pong(5) + "  \n")
        self.assertEqual(len(values), 1)

    def test_runaway_garbage_is_bounded(self) -> None:
        asm = BleNotificationAssembler(max_buffer=64)
        self.assertEqual(asm.push("{" + "x" * 200), [])
        self.assertLessEqual(len(asm.buffered), 64)
        # Still usable afterwards.
        values = asm.push(_pong(6))
        self.assertEqual(len(values), 1)

    def test_reset_clears_partial_state(self) -> None:
        asm = BleNotificationAssembler()
        asm.push(_pong(8)[:20])
        self.assertNotEqual(asm.buffered, "")
        asm.reset()
        self.assertEqual(asm.buffered, "")
        values = asm.push(_pong(9))
        self.assertEqual(len(values), 1)
        self.assertEqual(values[0]["id"], 9)


if __name__ == "__main__":
    unittest.main()
