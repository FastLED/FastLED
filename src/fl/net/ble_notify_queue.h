#pragma once

/// @file ble_notify_queue.h
/// @brief Bounded outbound FIFO for chunked BLE GATT notifications.
///
/// A GATT notification carries at most `ATT_MTU - 3` bytes, so a JSON-RPC
/// response usually needs several sends. The transport therefore holds a
/// payload across multiple `can-send-now` callbacks while it drains.
///
/// Storing that in-flight payload in a single buffer loses data: when a
/// second response is produced before the first finishes draining, the
/// sink overwrites the buffer and the peer receives a truncated first
/// response followed by the whole second one (FastLED#3955). This queue
/// keeps completed responses in order and only advances to the next one
/// after the final chunk of the current one is accepted.
///
/// The queue is deliberately platform-independent — no BTstack, no NimBLE,
/// no ATT types — so the ordering and chunking contract is covered by host
/// unit tests (`tests/fl/net/ble_notify_queue.cpp`) on targets that have no
/// BLE silicon.

#include "fl/stl/int.h"
#include "fl/stl/vector.h"

namespace fl {
namespace net {
namespace ble {

/// Bounded FIFO of outbound notification payloads, drained in MTU-sized
/// chunks.
///
/// Exactly one payload is "in flight" at a time: `chunkData()` /
/// `chunkSize()` always describe the front payload at the current offset,
/// and the front is popped only once `advance()` has consumed all of it.
class BleNotifyQueue {
  public:
    /// Maximum number of complete responses held at once.
    ///
    /// Responses are produced by the RPC layer one request at a time, so a
    /// depth of 4 absorbs a burst without letting a peer that never drains
    /// grow the heap without bound. Pushing past this fails rather than
    /// dropping an already-queued response — losing the *oldest* response
    /// would corrupt an transfer already partly on the wire.
    static constexpr size_t kCapacity = 4;

    /// Append a complete payload.
    ///
    /// @return false when the queue is full; the caller should report the
    ///         drop rather than silently losing the response.
    bool push(const char *data, size_t size) {
        if (data == nullptr || size == 0 || mCount >= kCapacity) {
            return false;
        }
        fl::vector<u8> &slot = mSlots[mTail];
        slot.clear();
        slot.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            slot.push_back(static_cast<u8>(data[i]));
        }
        mTail = (mTail + 1) % kCapacity;
        ++mCount;
        return true;
    }

    /// True when nothing is waiting to be sent.
    bool empty() const { return mCount == 0; }

    /// Number of complete payloads queued, including the one in flight.
    size_t depth() const { return mCount; }

    /// Bytes of the in-flight payload not yet accepted by the stack.
    size_t remaining() const {
        if (mCount == 0) {
            return 0;
        }
        const size_t size = mSlots[mHead].size();
        return mOffset >= size ? 0 : size - mOffset;
    }

    /// Start of the next chunk, or nullptr when the queue is empty.
    const u8 *chunkData() const {
        if (remaining() == 0) {
            return nullptr;
        }
        return mSlots[mHead].data() + mOffset;
    }

    /// Size of the next chunk, capped at @p max_payload.
    ///
    /// Never rounds up past what is left of the in-flight payload, so a
    /// caller can hand the result straight to `att_server_notify()`.
    size_t chunkSize(size_t max_payload) const {
        const size_t left = remaining();
        if (max_payload == 0) {
            return 0;
        }
        return left < max_payload ? left : max_payload;
    }

    /// Record that @p sent bytes were accepted by the stack.
    ///
    /// Pops the front payload once it has been fully sent, so the next
    /// `chunkData()` describes the following response.
    void advance(size_t sent) {
        if (mCount == 0 || sent == 0) {
            return;
        }
        const size_t size = mSlots[mHead].size();
        mOffset += sent;
        if (mOffset >= size) {
            mSlots[mHead].clear();
            mHead = (mHead + 1) % kCapacity;
            --mCount;
            mOffset = 0;
        }
    }

    /// Drop every queued payload — used when the link goes away.
    void clear() {
        for (size_t i = 0; i < kCapacity; ++i) {
            mSlots[i].clear();
        }
        mHead = 0;
        mTail = 0;
        mCount = 0;
        mOffset = 0;
    }

  private:
    fl::vector<u8> mSlots[kCapacity];
    size_t mHead = 0;
    size_t mTail = 0;
    size_t mCount = 0;
    /// Bytes of `mSlots[mHead]` already accepted by the stack.
    size_t mOffset = 0;
};

} // namespace ble
} // namespace net
} // namespace fl
