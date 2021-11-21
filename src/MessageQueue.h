#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <deque>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

// MessageQueue: condition-variable synchronized deque accessed at the back.
//
// Approach
//   std::deque with push_back / back+pop_back under a single mutex.
//   A condition_variable blocks receivers until an element is available or
//   the queue is shut down.
//
// Ordering: LIFO (most recently sent element is received first).
//
// Complexity
//   send              O(1) amortized   lock + push_back + notify_one
//   receive           O(1) amortized   wait + back + pop_back
//   shutdown          O(1)             lock + flag + notify_all
//   size / empty      O(1)             lock + deque::size
//
// Space: O(n) for n queued elements plus one mutex and one condvar.
//
// Thread safety: all public methods are safe to call from any thread.
// shutdown() is idempotent.  After shutdown, send() throws
// QueueClosedException; receive() drains remaining elements first, then
// throws QueueClosedException when the queue is empty.

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
