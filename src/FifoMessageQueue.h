#ifndef FIFO_MESSAGE_QUEUE_H
#define FIFO_MESSAGE_QUEUE_H

#include <queue>
#include <condition_variable>
#include <mutex>

template <class T>
class FifoMessageQueue
{
public:
    void send(T &&msg)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        _queue.push(std::move(msg));
        _cond_var.notify_one();
    }

    T receive()
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cond_var.wait(lock, [this] { return !_queue.empty(); });

        T msg = std::move(_queue.front());
        _queue.pop();

        return msg;
    }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(_mtx);
        return _queue.size();
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(_mtx);
        return _queue.empty();
    }

private:
    std::queue<T> _queue;
    std::condition_variable _cond_var;
    mutable std::mutex _mtx;
};

#endif
