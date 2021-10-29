#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <deque>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

class QueueClosedException : public std::runtime_error
{
public:
    QueueClosedException() : std::runtime_error("queue is closed") {}
};

template <class T>
class MessageQueue
{
public:
    void send(T &&msg)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_closed)
            throw QueueClosedException();
        _queue.push_back(std::move(msg));
        _cond_var.notify_one();
    }

    T receive()
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cond_var.wait(lock, [this] { return !_queue.empty() || _closed; });

        if (_queue.empty() && _closed)
            throw QueueClosedException();

        T msg = std::move(_queue.back());
        _queue.pop_back();

        return msg;
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
    std::deque<T> _queue;
    std::condition_variable _cond_var;
    mutable std::mutex _mtx;
    bool _closed = false;
};

#endif
