// Host-side tests for the outbound BLE notification FIFO (FastLED#3955).
//
// The RP2350W BTstack transport drains a JSON-RPC response over several
// `can-send-now` callbacks because one notification carries at most
// `ATT_MTU - 3` bytes. Before this queue existed the transport held that
// in-flight payload in a single buffer, so a second response produced
// mid-transfer overwrote the first and the peer saw a truncated message.
//
// These tests pin the ordering + chunking contract without BTstack, so
// they run on the host where there is no BLE silicon.

#include "test.h"
#include "fl/net/ble_notify_queue.h"
#include "fl/stl/string.h"

namespace {

using fl::net::ble::BleNotifyQueue;

// Drain up to `max_payload` bytes at a time, mimicking the transport's
// can-send-now loop, and return everything the peer would observe.
fl::string drain(BleNotifyQueue &queue, size_t max_payload) {
    fl::string seen;
    while (!queue.empty()) {
        const size_t chunk = queue.chunkSize(max_payload);
        FL_REQUIRE(chunk > 0);
        const fl::u8 *data = queue.chunkData();
        FL_REQUIRE(data != nullptr);
        for (size_t i = 0; i < chunk; ++i) {
            seen.push_back(static_cast<char>(data[i]));
        }
        queue.advance(chunk);
    }
    return seen;
}

void pushString(BleNotifyQueue &queue, const fl::string &value) {
    FL_CHECK(queue.push(value.c_str(), value.size()));
}

} // namespace

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("ble notify queue: second response does not truncate the first") {
    // The regression the issue asks for: two complete responses are
    // produced before the first transfer finishes.
    BleNotifyQueue queue;
    const fl::string first("REMOTE: {\"id\":1,\"result\":{\"message\":\"pong\"}}");
    const fl::string second("REMOTE: {\"id\":2,\"result\":{\"message\":\"ack\"}}");

    pushString(queue, first);

    // Send one MTU-sized chunk, leaving the rest of `first` unsent.
    const size_t mtu_payload = 20;
    const size_t sent = queue.chunkSize(mtu_payload);
    FL_CHECK_EQ(sent, mtu_payload);
    queue.advance(sent);
    FL_CHECK(queue.remaining() > 0);

    // A second response arrives mid-transfer. It must queue behind the
    // first rather than replace it.
    pushString(queue, second);
    FL_CHECK_EQ(queue.depth(), size_t(2));

    fl::string rest = drain(queue, mtu_payload);
    fl::string expected_rest = first.substr(mtu_payload) + second;
    FL_CHECK_EQ(rest, expected_rest);
    FL_CHECK(queue.empty());
}

FL_TEST_CASE("ble notify queue: preserves order across many responses") {
    BleNotifyQueue queue;
    pushString(queue, fl::string("aaa"));
    pushString(queue, fl::string("bb"));
    pushString(queue, fl::string("cccc"));

    FL_CHECK_EQ(queue.depth(), size_t(3));
    FL_CHECK_EQ(drain(queue, 2), fl::string("aaabbcccc"));
}

FL_TEST_CASE("ble notify queue: chunking never overruns the payload") {
    BleNotifyQueue queue;
    pushString(queue, fl::string("12345"));

    // A generous MTU must not report more than what is left.
    FL_CHECK_EQ(queue.chunkSize(1000), size_t(5));
    queue.advance(2);
    FL_CHECK_EQ(queue.chunkSize(1000), size_t(3));
    FL_CHECK_EQ(queue.remaining(), size_t(3));
    // A zero-sized window yields nothing rather than a bogus send.
    FL_CHECK_EQ(queue.chunkSize(0), size_t(0));

    queue.advance(3);
    FL_CHECK(queue.empty());
    FL_CHECK(queue.chunkData() == nullptr);
    FL_CHECK_EQ(queue.chunkSize(1000), size_t(0));
}

FL_TEST_CASE("ble notify queue: is bounded and reports the drop") {
    BleNotifyQueue queue;
    for (size_t i = 0; i < BleNotifyQueue::kCapacity; ++i) {
        FL_CHECK(queue.push("x", 1));
    }
    FL_CHECK_EQ(queue.depth(), BleNotifyQueue::kCapacity);

    // Full: the newest response is refused rather than evicting one that
    // may already be partly on the wire.
    FL_CHECK_FALSE(queue.push("y", 1));
    FL_CHECK_EQ(queue.depth(), BleNotifyQueue::kCapacity);

    // Draining one frees exactly one slot.
    queue.advance(queue.chunkSize(64));
    FL_CHECK(queue.push("y", 1));
}

FL_TEST_CASE("ble notify queue: empty and degenerate pushes are rejected") {
    BleNotifyQueue queue;
    FL_CHECK_FALSE(queue.push(nullptr, 4));
    FL_CHECK_FALSE(queue.push("x", 0));
    FL_CHECK(queue.empty());
    // Advancing an empty queue must be a no-op, not an underflow.
    queue.advance(5);
    FL_CHECK(queue.empty());
    FL_CHECK_EQ(queue.remaining(), size_t(0));
}

FL_TEST_CASE("ble notify queue: dropFront skips an undeliverable payload") {
    // A payload the peer keeps refusing must not block the ones behind it.
    BleNotifyQueue queue;
    pushString(queue, fl::string("stuck"));
    pushString(queue, fl::string("next"));

    queue.advance(2); // partway through "stuck" before it starts failing
    queue.dropFront();

    FL_CHECK_EQ(queue.depth(), size_t(1));
    // The survivor must start from offset zero, not inherit the dropped
    // payload's progress.
    FL_CHECK_EQ(drain(queue, 64), fl::string("next"));
}

FL_TEST_CASE("ble notify queue: dropFront on an empty queue is a no-op") {
    BleNotifyQueue queue;
    queue.dropFront();
    FL_CHECK(queue.empty());
    pushString(queue, fl::string("ok"));
    FL_CHECK_EQ(drain(queue, 64), fl::string("ok"));
}

FL_TEST_CASE("ble notify queue: slots are reused across many cycles") {
    // Exercises the ring wrap: far more payloads than there are slots.
    BleNotifyQueue queue;
    for (int i = 0; i < 20; ++i) {
        pushString(queue, fl::string("payload"));
        FL_CHECK_EQ(queue.depth(), size_t(1));
        FL_CHECK_EQ(drain(queue, 3), fl::string("payload"));
        FL_CHECK(queue.empty());
    }
}

FL_TEST_CASE("ble notify queue: clear drops in-flight state on disconnect") {
    BleNotifyQueue queue;
    pushString(queue, fl::string("first"));
    pushString(queue, fl::string("second"));
    queue.advance(2); // partway through `first`

    queue.clear();

    FL_CHECK(queue.empty());
    FL_CHECK_EQ(queue.depth(), size_t(0));
    FL_CHECK_EQ(queue.remaining(), size_t(0));
    FL_CHECK(queue.chunkData() == nullptr);

    // A reconnect reuses the queue and must start from a clean offset.
    pushString(queue, fl::string("after"));
    FL_CHECK_EQ(drain(queue, 64), fl::string("after"));
}

}
