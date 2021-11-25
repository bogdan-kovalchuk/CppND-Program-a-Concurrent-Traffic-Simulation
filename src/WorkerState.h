#ifndef WORKER_STATE_H
#define WORKER_STATE_H

#include <atomic>
#include <condition_variable>
#include <mutex>

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
        std::lock_guard<std::mutex> lock(_mtx);
        _running.store(true, std::memory_order_release);
    }

private:
    std::atomic<bool> _running;
    std::mutex _mtx;
    std::condition_variable _cv;
};

#endif
