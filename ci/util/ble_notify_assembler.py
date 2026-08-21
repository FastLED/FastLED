"""Reassemble ATT-fragmented BLE notifications into complete JSON values.

The device chunks each JSON-RPC response into ``ATT_MTU - 3`` byte
notifications, so one logical response can arrive as several notification
values, and several responses can arrive back to back. The only message
marker on the wire is the ``REMOTE: `` prefix each response starts with.

The naive reader concatenated every notification into one buffer that was
never reset, so a single unparseable value poisoned the buffer and every
later response — including a valid ``pong`` — failed to parse forever
(FastLED#3955). This assembler instead consumes exactly the bytes that
form each complete JSON value, and resynchronizes only at a real message
boundary: the start of a fragment.

Resyncing on *any* occurrence of the marker inside the buffer would be
wrong, because a payload may legitimately contain the literal
``REMOTE: `` inside a JSON string (the device echoes host lines that
carry that prefix). Searching the buffer would then chop a valid
half-arrived message in two. A response only ever *begins* at the start
of a fragment, so that is the only place a boundary is honored.

Deliberately free of any ``bleak`` import so the parsing contract can be
unit-tested on a host without BLE support.
"""

from __future__ import annotations

import json
from typing import Any


REMOTE_PREFIX = "REMOTE: "

# Cap on unparseable backlog. A response is a few hundred bytes, so a
# buffer this large means the stream desynchronized; dropping it is better
# than growing without bound on a chatty or corrupt link.
MAX_BUFFER_CHARS = 16384

_DECODER = json.JSONDecoder()


class BleNotificationAssembler:
    """Feed notification fragments in, get complete JSON values out."""

    def __init__(
        self, prefix: str = REMOTE_PREFIX, max_buffer: int = MAX_BUFFER_CHARS
    ) -> None:
        self._prefix = prefix
        self._max_buffer = max_buffer
        self._buffer = ""

    @property
    def buffered(self) -> str:
        """Bytes held back waiting for the rest of a value (test visibility)."""
        return self._buffer

    def reset(self) -> None:
        """Drop partial state — e.g. after a disconnect."""
        self._buffer = ""

    def push(self, fragment: str) -> list[Any]:
        """Add one notification value; return every JSON value it completed.

        Returns an empty list while a value is still incomplete. Values are
        returned in arrival order, so a valid response is still delivered
        when it follows a malformed one.
        """
        if not fragment:
            return []

        # A fragment starting with the marker begins a new response. Anything
        # still buffered at that point never completed, so drop it rather
        # than let it swallow the message that is starting now.
        if fragment.lstrip().startswith(self._prefix) and self._buffer:
            self._buffer = ""

        self._buffer += fragment

        values: list[Any] = []
        while True:
            buf = self._buffer.lstrip()
            if buf.startswith(self._prefix):
                buf = buf[len(self._prefix) :].lstrip()
            if not buf:
                self._buffer = ""
                break

            try:
                value, end = _DECODER.raw_decode(buf)
            except ValueError:
                # Still arriving, or garbage. Either way it can only be
                # resolved by the next fragment that starts a new message;
                # hold it until then, bounded so a corrupt link cannot grow
                # the buffer without limit.
                self._buffer = "" if len(buf) > self._max_buffer else buf
                break

            values.append(value)
            self._buffer = buf[end:]

        return values
