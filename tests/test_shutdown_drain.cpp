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

template <typename Q>
void test_partial_drain_under_shutdown_impl()
{
    Q q;
    const int total = 10;
    for (int i = 0; i < total; ++i)
    {
        int v = i;
        q.send(std::move(v));
    }

    q.shutdown();

    std::vector<int> drained;
    try
    {
        while (true)
        {
            drained.push_back(q.receive());
        }
    }
    catch (const QueueClosedException &) {}

    ASSERT_EQ(static_cast<int>(drained.size()), total);
}

TEST(lifo_partial_drain_all_delivered)
{
    test_partial_drain_under_shutdown_impl<MessageQueue<int>>();
}

TEST(fifo_partial_drain_all_delivered)
{
    test_partial_drain_under_shutdown_impl<FifoMessageQueue<int>>();
}

template <typename Q>
void test_drain_ordering_impl()
{
    Q q;
    for (int i = 0; i < 5; ++i)
    {
        int v = i;
        q.send(std::move(v));
    }

    q.shutdown();

    std::vector<int> drained;
    try
    {
        while (true)
        {
            drained.push_back(q.receive());
        }
    }
    catch (const QueueClosedException &) {}

    ASSERT_EQ(static_cast<int>(drained.size()), 5);
}

TEST(fifo_drain_preserves_insertion_order)
{
    FifoMessageQueue<int> q;
    for (int i = 0; i < 5; ++i)
    {
        int v = i;
        q.send(std::move(v));
    }
    q.shutdown();

    std::vector<int> drained;
    try
    {
        while (true)
        {
            drained.push_back(q.receive());
        }
    }
    catch (const QueueClosedException &) {}

    ASSERT_EQ(static_cast<int>(drained.size()), 5);
    for (int i = 0; i < 5; ++i)
    {
        ASSERT_EQ(drained[i], i);
    }
}

TEST(lifo_drain_reverses_insertion_order)
{
    MessageQueue<int> q;
    for (int i = 0; i < 5; ++i)
    {
        int v = i;
        q.send(std::move(v));
    }
    q.shutdown();

    std::vector<int> drained;
    try
    {
        while (true)
        {
            drained.push_back(q.receive());
        }
    }
    catch (const QueueClosedException &) {}

    ASSERT_EQ(static_cast<int>(drained.size()), 5);
    for (int i = 0; i < 5; ++i)
    {
        ASSERT_EQ(drained[i], 4 - i);
    }
}

template <typename Q>
void test_concurrent_drain_completeness_impl()
{
    Q q;
    const int total = 200;
    std::atomic<int> sent{0};

    std::thread producer([&] {
        for (int i = 0; i < total; ++i)
        {
            int v = i;
            q.send(std::move(v));
            sent.fetch_add(1);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        q.shutdown();
    });

    std::atomic<int> received{0};
    std::thread consumer([&] {
        try
        {
            while (true)
            {
                q.receive();
                received.fetch_add(1);
            }
        }
        catch (const QueueClosedException &) {}
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.load(), total);
}

TEST(lifo_concurrent_drain_completeness)
{
    test_concurrent_drain_completeness_impl<MessageQueue<int>>();
}

TEST(fifo_concurrent_drain_completeness)
{
    test_concurrent_drain_completeness_impl<FifoMessageQueue<int>>();
}

TEST(drain_after_shutdown_no_new_sends)
{
    FifoMessageQueue<int> q;
    for (int i = 0; i < 3; ++i)
    {
        int v = i;
        q.send(std::move(v));
    }
    q.shutdown();

    int drained = 0;
    try
    {
        while (true)
        {
            q.receive();
            ++drained;
        }
    }
    catch (const QueueClosedException &) {}

    ASSERT_EQ(drained, 3);

    int v = 99;
    bool threw = false;
    try { q.send(std::move(v)); }
    catch (const QueueClosedException &) { threw = true; }
    ASSERT_TRUE(threw);
}

int main()
{
    std::cout << "Shutdown drain verification tests:\n";

    RUN(lifo_partial_drain_all_delivered);
    RUN(fifo_partial_drain_all_delivered);
    RUN(fifo_drain_preserves_insertion_order);
    RUN(lifo_drain_reverses_insertion_order);
    RUN(lifo_concurrent_drain_completeness);
    RUN(fifo_concurrent_drain_completeness);
    RUN(drain_after_shutdown_no_new_sends);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
