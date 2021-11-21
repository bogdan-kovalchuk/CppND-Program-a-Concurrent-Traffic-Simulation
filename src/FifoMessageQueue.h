#ifndef FIFO_MESSAGE_QUEUE_H
#define FIFO_MESSAGE_QUEUE_H

#include <queue>
#include <condition_variable>
#include <mutex>
#include "MessageQueue.h"

// FifoMessageQueue: condition-variable synchronized FIFO queue.
//
// Approach
//   std::queue (deque-backed) with push / front+pop under a single mutex.
//   A condition_variable blocks receivers until an element is available or
//   the queue is shut down.
//
// Ordering: FIFO (first sent element is received first).
//
// Complexity
//   send              O(1) amortized   lock + push + notify_one
//   receive           O(1) amortized   wait + front + pop
//   shutdown          O(1)             lock + flag + notify_all
//   size / empty      O(1)             lock + queue::size
//
// Space: O(n) for n queued elements plus one mutex and one condvar.
//
// Thread safety: identical guarantees to MessageQueue<T>.
// shutdown() is idempotent.  After shutdown, send() throws
// QueueClosedException; receive() drains remaining elements first, then
// throws when the internal buffer is exhausted.
//
// Comparison with MessageQueue<T>
//   MessageQueue accesses the back of the deque (LIFO), which suits
//   "latest-state" patterns where only the newest value matters.
//   FifoMessageQueue preserves insertion order, which is required when
//   every message must be processed in the sequence it was produced
//   (e.g. vehicle-entry permits at an intersection).

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
