#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/function.h"
#include "fl/stl/priority_queue.h"

namespace fl {

/**
 * @brief Generic time-based task scheduler
 *
 * Executes tasks at specified timestamps. Tasks are stored in a stable priority queue,
 * ensuring FIFO ordering for tasks scheduled at the same time.
 *
 * Usage:
 *   RpcScheduler<> scheduler;
 *   scheduler.schedule(1000, []() { doSomething(); });
 *   scheduler.tick(millis());  // Execute ready tasks
 */
template<typename Task = fl::function<void()>>
class RpcScheduler {
public:
    RpcScheduler() = default;
    ~RpcScheduler() = default;

    // Non-copyable but movable
    RpcScheduler(const RpcScheduler&) = delete;
    RpcScheduler& operator=(const RpcScheduler&) = delete;
    RpcScheduler(RpcScheduler&&) = default;
    RpcScheduler& operator=(RpcScheduler&&) = default;

    /**
     * @brief Schedule a task for execution at specified timestamp
     * @param timestamp Execution time (e.g., millis())
     * @param task Callable to execute when timestamp arrives
     */
    void schedule(u32 timestamp, Task task) {
        mQueue.push({timestamp, fl::move(task)});
    }

    /**
     * @brief Execute all tasks with timestamp <= currentTime
     * @param currentTime Current time (e.g., millis())
     * @return Number of tasks executed
     */
    size_t tick(u32 currentTime) {
        size_t executed = 0;

        while (!mQueue.empty() && currentTime >= mQueue.top().executeAt) {
            // Copy the task before popping (can't execute after pop)
            ScheduledTask scheduledTask = mQueue.top();
            mQueue.pop();
            scheduledTask.task();  // Execute task
            executed++;
        }

        return executed;
    }

    /**
     * @brief Get number of pending scheduled tasks
     * @return Count of tasks waiting to be executed
     */
    size_t pendingCount() const {
        return mQueue.size();
    }

    /**
     * @brief Clear all scheduled tasks
     */
    void clear() {
        mQueue.clear();
    }

private:
    struct ScheduledTask {
        u32 executeAt;      // Timestamp when to execute
        Task task;          // Task to execute

        // Comparison for stable priority queue (earlier times = higher priority)
        // priority_queue_stable uses fl::less by default, creating a max-heap
        // Invert comparison so earlier (smaller) timestamps are "greater" = higher priority
        bool operator<(const ScheduledTask& other) const {
            return executeAt > other.executeAt;  // Inverted: smaller timestamps = higher priority
        }
    };

    fl::priority_queue_stable<ScheduledTask> mQueue;
};

} // namespace fl
