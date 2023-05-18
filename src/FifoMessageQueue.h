#ifndef FIFO_MESSAGE_QUEUE_H
#define FIFO_MESSAGE_QUEUE_H

#include <queue>
#include <vector>
#include <condition_variable>
#include <mutex>
#include "MessageQueue.h"

template <class T>
class FifoMessageQueue
{
public:
    void send(T &&msg)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_closed)
            throw QueueClosedException();
        _queue.push(std::move(msg));
        _cond_var.notify_one();
    }

    bool try_send(T &&msg)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_closed)
            return false;
        _queue.push(std::move(msg));
        _cond_var.notify_one();
        return true;
    }

    T receive()
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cond_var.wait(lock, [this] { return !_queue.empty() || _closed; });

        if (_queue.empty() && _closed)
            throw QueueClosedException();

        T msg = std::move(_queue.front());
        _queue.pop();

        return msg;
    }

    std::vector<T> drain()
    {
        std::lock_guard<std::mutex> lock(_mtx);
        std::vector<T> result;
        result.reserve(_queue.size());
        while (!_queue.empty())
        {
            result.push_back(std::move(_queue.front()));
            _queue.pop();
        }
        return result;
    }

    void shutdown()
    {
        std::lock_guard<std::mutex> lock(_mtx);
        _closed = true;
        _cond_var.notify_all();
    }

    bool is_closed() const
    {
        std::lock_guard<std::mutex> lock(_mtx);
        return _closed;
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
    bool _closed = false;
};

#endif
