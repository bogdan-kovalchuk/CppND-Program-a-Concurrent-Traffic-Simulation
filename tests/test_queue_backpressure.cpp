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

TEST(test_many_producers_single_consumer)
{
    FifoMessageQueue<int> q;
    const int num_producers = 8;
    const int msgs_per_producer = 200;
    const int total = num_producers * msgs_per_producer;
    std::atomic<int> consumed{0};

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p)
    {
        producers.emplace_back([&, p] {
            for (int i = 0; i < msgs_per_producer; ++i)
            {
                int v = p * msgs_per_producer + i;
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

TEST(test_queue_growth_under_burst)
{
    MessageQueue<int> q;
    const int burst_size = 500;

    std::thread producer([&] {
        for (int i = 0; i < burst_size; ++i)
        {
            int v = i;
            q.send(std::move(v));
        }
    });

    producer.join();
    ASSERT_EQ(q.size(), static_cast<std::size_t>(burst_size));

    std::vector<int> drained;
    for (int i = 0; i < burst_size; ++i)
    {
        drained.push_back(q.receive());
    }
    ASSERT_TRUE(q.empty());
    ASSERT_EQ(static_cast<int>(drained.size()), burst_size);
}

TEST(test_consumer_catchup_after_producer_burst)
{
    FifoMessageQueue<int> q;
    const int n = 300;

    std::thread producer([&] {
        for (int i = 0; i < n; ++i)
        {
            int v = i;
            q.send(std::move(v));
        }
    });

    producer.join();

    std::atomic<int> received{0};
    std::thread consumer([&] {
        for (int i = 0; i < n; ++i)
        {
            q.receive();
            received.fetch_add(1);
        }
    });

    consumer.join();
    ASSERT_EQ(received.load(), n);
    ASSERT_TRUE(q.empty());
}

TEST(test_sustained_producer_consumer_balance)
{
    MessageQueue<int> q;
    const int total = 1000;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    std::thread producer([&] {
        for (int i = 0; i < total; ++i)
        {
            int v = i;
            q.send(std::move(v));
            produced.fetch_add(1);
            if (i % 10 == 0)
                std::this_thread::sleep_for(std::chrono::microseconds(5));
        }
    });

    std::thread consumer([&] {
        while (consumed.load() < total)
        {
            try
            {
                q.receive();
                consumed.fetch_add(1);
            }
            catch (const QueueClosedException &)
            {
                break;
            }
        }
    });

    producer.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    q.shutdown();
    consumer.join();

    ASSERT_EQ(produced.load(), total);
    ASSERT_EQ(consumed.load(), total);
}

TEST(test_drain_batch_under_load)
{
    FifoMessageQueue<int> q;
    const int n = 200;

    std::thread producer([&] {
        for (int i = 0; i < n; ++i)
        {
            int v = i;
            q.send(std::move(v));
        }
    });

    producer.join();

    auto batch = q.drain();
    ASSERT_EQ(static_cast<int>(batch.size()), n);
    ASSERT_TRUE(q.empty());

    for (int i = 0; i < n; ++i)
    {
        ASSERT_EQ(batch[i], i);
    }
}

int main()
{
    std::cout << "Queue backpressure tests:\n";

    RUN(test_many_producers_single_consumer);
    RUN(test_queue_growth_under_burst);
    RUN(test_consumer_catchup_after_producer_burst);
    RUN(test_sustained_producer_consumer_balance);
    RUN(test_drain_batch_under_load);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
