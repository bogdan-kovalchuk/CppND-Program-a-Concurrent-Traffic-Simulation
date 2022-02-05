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

TEST(test_alternating_producer_consumer)
{
    MessageQueue<int> q;
    const int n = 100;
    std::atomic<int> consumed{0};

    std::thread producer([&] {
        for (int i = 0; i < n; ++i)
        {
            int v = i;
            q.send(std::move(v));
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });

    std::thread consumer([&] {
        for (int i = 0; i < n; ++i)
        {
            q.receive();
            consumed.fetch_add(1);
        }
    });

    producer.join();
    consumer.join();
    ASSERT_EQ(consumed.load(), n);
    ASSERT_TRUE(q.empty());
}

TEST(test_fifo_concurrent_send_receive_integrity)
{
    FifoMessageQueue<int> q;
    const int per_producer = 50;
    const int num_producers = 4;
    const int total = per_producer * num_producers;
    std::atomic<int> consumed{0};

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p)
    {
        producers.emplace_back([&, p] {
            for (int i = 0; i < per_producer; ++i)
            {
                int v = p * per_producer + i;
                q.send(std::move(v));
            }
        });
    }

    std::thread consumer([&] {
        for (int i = 0; i < total; ++i)
        {
            q.receive();
            consumed.fetch_add(1);
        }
    });

    for (auto &t : producers) t.join();
    consumer.join();

    ASSERT_EQ(consumed.load(), total);
    ASSERT_TRUE(q.empty());
}

TEST(test_multiple_consumers_all_served)
{
    MessageQueue<int> q;
    const int n = 200;
    const int num_consumers = 4;
    std::atomic<int> total_consumed{0};

    std::thread producer([&] {
        for (int i = 0; i < n; ++i)
        {
            int v = i;
            q.send(std::move(v));
        }
    });

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c)
    {
        consumers.emplace_back([&] {
            while (true)
            {
                try
                {
                    q.receive();
                    total_consumed.fetch_add(1);
                }
                catch (const QueueClosedException &)
                {
                    break;
                }
            }
        });
    }

    producer.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    q.shutdown();

    for (auto &t : consumers) t.join();
    ASSERT_EQ(total_consumed.load(), n);
}

TEST(test_send_receive_under_contention)
{
    FifoMessageQueue<int> q;
    const int ops_per_thread = 100;
    const int num_senders = 3;
    const int num_receivers = 3;
    const int total_msgs = ops_per_thread * num_senders;
    std::atomic<int> received_count{0};

    std::vector<std::thread> senders;
    for (int s = 0; s < num_senders; ++s)
    {
        senders.emplace_back([&, s] {
            for (int i = 0; i < ops_per_thread; ++i)
            {
                int v = s * ops_per_thread + i;
                q.send(std::move(v));
            }
        });
    }

    std::vector<std::thread> receivers;
    for (int r = 0; r < num_receivers; ++r)
    {
        receivers.emplace_back([&] {
            while (received_count.load() < total_msgs)
            {
                try
                {
                    q.receive();
                    received_count.fetch_add(1);
                }
                catch (const QueueClosedException &)
                {
                    break;
                }
            }
        });
    }

    for (auto &t : senders) t.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    q.shutdown();
    for (auto &t : receivers) t.join();

    ASSERT_EQ(received_count.load(), total_msgs);
}

int main()
{
    std::cout << "Queue synchronization tests:\n";

    RUN(test_alternating_producer_consumer);
    RUN(test_fifo_concurrent_send_receive_integrity);
    RUN(test_multiple_consumers_all_served);
    RUN(test_send_receive_under_contention);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
