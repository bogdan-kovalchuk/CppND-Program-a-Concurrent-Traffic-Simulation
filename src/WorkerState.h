#ifndef WORKER_STATE_H
#define WORKER_STATE_H

#include <atomic>
#include <condition_variable>
#include <mutex>

// WorkerState: tracks whether a worker thread should continue running.
//
// Approach
//   An atomic flag indicates running/stopped state. A condition variable
//   allows workers to sleep until explicitly stopped or until they need
//   to wake for other reasons (e.g. new work available).
//
// Complexity
//   start/stop      O(1)   atomic store + notify_all
//   is_running      O(1)   atomic load
//   wait_for_stop   O(1)   condition_variable wait with predicate
//
// Thread safety: all methods are safe to call from any thread.
// stop() is idempotent.
//
// Race conditions handled:
//   1. Concurrent stop() calls: atomic flag ensures only one transition
//   2. stop() during wait_for_stop(): notify_all wakes all waiters
//   3. is_running() check before wait: predicate rechecks after wake
//   4. stop() before worker starts: initial state check prevents entry
//   5. Multiple workers: each checks its own state independently
//
// Usage pattern:
//   WorkerState state;
//   std::thread worker([&] {
//       while (state.is_running()) {
//           // do work
//           state.wait_for_stop(timeout);
//       }
//   });
//   // later...
//   state.stop();
//   worker.join();

class WorkerState
{
public:
    WorkerState() : _running(true) {}

    bool is_running() const
    {
        return _running.load(std::memory_order_acquire);
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _running.store(false, std::memory_order_release);
        }
        _cv.notify_all();
    }

    bool wait_for_stop(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        return _cv.wait_for(lock, timeout, [this] {
            return !_running.load(std::memory_order_acquire);
        });
    }

    void restart()
    {
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _running.store(true, std::memory_order_release);
        }
        _cv.notify_all();
    }

private:
    std::atomic<bool> _running;
    std::mutex _mtx;
    std::condition_variable _cv;
};

#endif
