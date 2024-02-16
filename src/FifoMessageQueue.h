#ifndef FIFO_MESSAGE_QUEUE_H
#define FIFO_MESSAGE_QUEUE_H

#include <queue>
#include <vector>
#include <condition_variable>
#include <limits>
#include <mutex>
#include "MessageQueue.h"

template <class T>
class FifoMessageQueue
{
public:
    explicit FifoMessageQueue(std::size_t capacity = std::numeric_limits<std::size_t>::max()) : _capacity(capacity)
    {
        if (_capacity == 0)
            throw std::invalid_argument("queue capacity must be greater than zero");
    }

    void send(T &&msg)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _not_full.wait(lock, [this] { return _queue.size() < _capacity || _closed; });
        if (_closed)
            throw QueueClosedException();
        _queue.push(std::move(msg));
        lock.unlock();
        _not_empty.notify_one();
    }

    bool try_send(T &&msg)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_closed || _queue.size() >= _capacity)
            return false;
        _queue.push(std::move(msg));
        _not_empty.notify_one();
        return true;
    }

    T receive()
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _not_empty.wait(lock, [this] { return !_queue.empty() || _closed; });

        if (_queue.empty() && _closed)
            throw QueueClosedException();

        T msg = std::move(_queue.front());
        _queue.pop();
        lock.unlock();
        _not_full.notify_one();
        return msg;
    }

    std::vector<T> drain()
    {
        std::unique_lock<std::mutex> lock(_mtx);
        std::vector<T> result;
        result.reserve(_queue.size());
        while (!_queue.empty())
        {
            result.push_back(std::move(_queue.front()));
            _queue.pop();
        }
        lock.unlock();
        _not_full.notify_all();
        return result;
    }

    void shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _closed = true;
        }
        _not_empty.notify_all();
        _not_full.notify_all();
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
    const std::size_t _capacity;
    std::condition_variable _not_empty;
    std::condition_variable _not_full;
    mutable std::mutex _mtx;
    bool _closed = false;
};

#endif
