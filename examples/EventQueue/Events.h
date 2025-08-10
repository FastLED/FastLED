#pragma once

#include "fl/priority_queue.h"
#include "fl/function.h"
#include "fl/time.h"
#include "fl/int.h"
#include "fl/functional.h"

// Simple event queue for examples only. Not part of the core library API.
struct EventItem {
    fl::u32 run_at;
    fl::u32 id;
    fl::function<void()> fn;
};

// Comparator that makes fl::PriorityQueue operate as a min-heap by time.
// Returns true when left has LOWER priority than right (so larger values sink).
struct EarlierFirst {
    bool operator()(const EventItem& a, const EventItem& b) const {
        if (a.run_at != b.run_at) {
            // For min-heap: parent should be replaced if it is greater than child
            return a.run_at > b.run_at;
        }
        // Stable order for equal timestamps
        return a.id > b.id;
    }
};

class Events {
public:
    Events() : _nextId(1) {}

    void add(fl::u32 at_ms, const fl::function<void()>& f) {
        _queue.push(EventItem{at_ms, _nextId++, f});
    }

    void add_after(fl::u32 delay_ms, const fl::function<void()>& f) {
        add(fl::time() + delay_ms, f);
    }

    void update() {
        const fl::u32 now = fl::time();
        while (!_queue.empty()) {
            const EventItem& current = _queue.top();
            // Handle wrap-around safely by casting to signed difference
            if (static_cast<fl::i32>(current.run_at - now) <= 0) {
                auto fn = current.fn;
                _queue.pop();
                if (fn) { fn(); }
            } else {
                break;
            }
        }
    }

    bool empty() const { return _queue.empty(); }
    fl::size size() const { return _queue.size(); }

private:
    fl::PriorityQueue<EventItem, EarlierFirst> _queue;
    fl::u32 _nextId;
};