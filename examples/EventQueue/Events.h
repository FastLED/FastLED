#pragma once

#include "fl/priority_queue.h"
#include "fl/function.h"
#include "fl/time.h"
#include "fl/int.h"
#include "fl/functional.h"
#include "fl/vector.h"

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

    int add(fl::u32 at_ms, const fl::function<void()>& f) {
        const fl::u32 id = _nextId++;
        _queue.push(EventItem{at_ms, id, f});
        return static_cast<int>(id);
    }

    int add_after(fl::u32 delay_ms, const fl::function<void()>& f) {
        return add(fl::time() + delay_ms, f);
    }

    bool cancel(int id) {
        _canceled.push_back(static_cast<fl::u32>(id));
        return true;
    }

    void update() {
        const fl::u32 now = fl::time();
        while (!_queue.empty()) {
            const EventItem& current = _queue.top();
            // Drop canceled jobs lazily
            if (is_canceled(current.id)) {
                _queue.pop();
                continue;
            }
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
    bool is_canceled(fl::u32 id) const {
        for (fl::size i = 0; i < _canceled.size(); ++i) {
            if (_canceled[i] == id) return true;
        }
        return false;
    }

    fl::PriorityQueue<EventItem, EarlierFirst> _queue;
    fl::vector<fl::u32> _canceled;
    fl::u32 _nextId;
};

// Manually-pumped runner for this example
class EventsRunner {
public:
    explicit EventsRunner(Events& events) : _events(events) {}

    // Manual pump
    void processEvents() { _events.update(); }

    // Scheduling API: return int id to allow cancellation
    int add(fl::u32 at_ms, const fl::function<void()>& f) { return _events.add(at_ms, f); }
    int add_after(fl::u32 delay_ms, const fl::function<void()>& f) { return _events.add_after(delay_ms, f); }
    bool cancel(int id) { return _events.cancel(id); }

    Events& events() { return _events; }

private:
    Events& _events;
};