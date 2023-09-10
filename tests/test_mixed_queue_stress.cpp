#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <set>
#include "MessageQueue.h"
#include "FifoMessageQueue.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name()
#define RUN(name) do { \
    std::cout << "  " #name "..."; \
    try { name(); std::cout << " PASSED\n"; ++tests_passed; } \
    catch (const std::exception &e) { std::cout << " FAILED: " << e.what() << "\n"; ++tests_failed; } \
    catch (...) { std::cout << " FAILED (unknown)\n"; ++tests_failed; } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a != _b) { \
        throw std::runtime_error(std::string("ASSERT_EQ failed: ") + std::to_string(_a) + " != " + std::to_string(_b) + " at line " + std::to_string(__LINE__)); \
    } \
} while(0)

#define ASSERT_TRUE(x) do { if (!(x)) throw std::runtime_error(std::string("ASSERT_TRUE failed at line ") + std::to_string(__LINE__)); } while(0)

TEST(test_parallel_fifo_and_lifo_queues)
{
    FifoMessageQueue<int> fifo_q;
    MessageQueue<int> lifo_q;
    const int n = 200;
    std::atomic<int> fifo_received{0};
    std::atomic<int> lifo_received{0};

    std::thread fifo_producer([&] {
        for (int i = 0; i < n; ++i)
        {
            int v = i;
            fifo_q.send(std::move(v));
        }
    });

    std::thread lifo_producer([&] {
        for (int i = 0; i < n; ++i)
        {
            int v = i;
            lifo_q.send(std::move(v));
        }
    });

    std::thread fifo_consumer([&] {
        for (int i = 0; i < n; ++i)
        {
            fifo_q.receive();
            fifo_received.fetch_add(1);
        }
    });

    std::thread lifo_consumer([&] {
        for (int i = 0; i < n; ++i)
        {
            lifo_q.receive();
            lifo_received.fetch_add(1);
        }
    });

    fifo_producer.join();
    lifo_producer.join();
    fifo_consumer.join();
    lifo_consumer.join();

    ASSERT_EQ(fifo_received.load(), n);
    ASSERT_EQ(lifo_received.load(), n);
    ASSERT_TRUE(fifo_q.empty());
    ASSERT_TRUE(lifo_q.empty());
}

TEST(test_concurrent_shutdown_of_multiple_queues)
{
    const int num_queues = 6;
    std::vector<MessageQueue<int>*> queues;
    for (int i = 0; i < num_queues; ++i)
    {
        queues.push_back(new MessageQueue<int>());
    }

    std::atomic<int> unblocked{0};
    std::vector<std::thread> receivers;
    for (int i = 0; i < num_queues; ++i)
    {
        receivers.emplace_back([&, i] {
            try { queues[i]->receive(); }
            catch (const QueueClosedException &) { unblocked.fetch_add(1); }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    std::vector<std::thread> shutdowns;
    for (int i = 0; i < num_queues; ++i)
    {
        shutdowns.emplace_back([&, i] {
            queues[i]->shutdown();
        });
    }

    for (auto &t : shutdowns) t.join();
    for (auto &t : receivers) t.join();

    ASSERT_EQ(unblocked.load(), num_queues);

    for (auto q : queues) delete q;
}

TEST(test_try_send_under_concurrent_shutdown)
{
    FifoMessageQueue<int> q;
    const int num_threads = 4;
    std::atomic<int> ok{0};
    std::atomic<int> fail{0};

    const int sends_per_thread = 2000;
    std::atomic<int> attempts{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([&] {
            for (int i = 0; i < sends_per_thread; ++i)
            {
                int v = i;
                if (q.try_send(std::move(v)))
                    ok.fetch_add(1);
                else
                    fail.fetch_add(1);
                attempts.fetch_add(1);
            }
        });
    }

    // Shut down only once the senders are genuinely in flight, so the
    // shutdown really does race concurrent try_send() calls. A fixed
    // wall-clock delay is not enough: sub-millisecond sleep_for() returns
    // immediately on some platforms, so every sender would finish before the
    // shutdown and the test would pass without exercising the race at all.
    while (attempts.load() < num_threads * sends_per_thread / 4)
    {
        std::this_thread::yield();
    }
    q.shutdown();

    for (auto &t : threads) t.join();
    ASSERT_EQ(ok.load() + fail.load(), num_threads * sends_per_thread);
    // The shutdown must actually have been observed by the senders, otherwise
    // this test would silently stop covering the race it is named after.
    ASSERT_TRUE(fail.load() > 0);
}

TEST(test_drain_empty_queue_returns_empty_vector)
{
    MessageQueue<int> q;
    auto batch = q.drain();
    ASSERT_EQ(static_cast<int>(batch.size()), 0);

    FifoMessageQueue<int> fq;
    auto fbatch = fq.drain();
    ASSERT_EQ(static_cast<int>(fbatch.size()), 0);
}

TEST(test_drain_after_partial_receive)
{
    FifoMessageQueue<int> q;
    for (int i = 0; i < 10; ++i)
    {
        int v = i;
        q.send(std::move(v));
    }

    int first = q.receive();
    ASSERT_EQ(first, 0);

    auto batch = q.drain();
    ASSERT_EQ(static_cast<int>(batch.size()), 9);
    ASSERT_EQ(batch[0], 1);
    ASSERT_EQ(batch[8], 9);
    ASSERT_TRUE(q.empty());
}

int main()
{
    std::cout << "Mixed queue stress tests:\n";

    RUN(test_parallel_fifo_and_lifo_queues);
    RUN(test_concurrent_shutdown_of_multiple_queues);
    RUN(test_try_send_under_concurrent_shutdown);
    RUN(test_drain_empty_queue_returns_empty_vector);
    RUN(test_drain_after_partial_receive);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
