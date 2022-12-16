#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <stdexcept>
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

#define ASSERT_THROWS(expr, exc_type) do { \
    bool caught = false; \
    try { expr; } catch (const exc_type &) { caught = true; } \
    if (!caught) throw std::runtime_error(std::string("expected exception not thrown at line ") + std::to_string(__LINE__)); \
} while(0)

template <typename Q>
void test_shutdown_unblocks_receiver_impl()
{
    Q q;
    std::atomic<bool> threw{false};

    std::thread receiver([&] {
        try { q.receive(); }
        catch (const QueueClosedException &) { threw.store(true); }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    q.shutdown();
    receiver.join();

    ASSERT_TRUE(threw.load());
    ASSERT_TRUE(q.is_closed());
}

TEST(lifo_shutdown_unblocks_receiver)
{
    test_shutdown_unblocks_receiver_impl<MessageQueue<int>>();
}

TEST(fifo_shutdown_unblocks_receiver)
{
    test_shutdown_unblocks_receiver_impl<FifoMessageQueue<int>>();
}

template <typename Q>
void test_send_after_shutdown_impl()
{
    Q q;
    q.shutdown();

    int v = 42;
    ASSERT_THROWS(q.send(std::move(v)), QueueClosedException);
}

TEST(lifo_send_after_shutdown_throws)
{
    test_send_after_shutdown_impl<MessageQueue<int>>();
}

TEST(fifo_send_after_shutdown_throws)
{
    test_send_after_shutdown_impl<FifoMessageQueue<int>>();
}

template <typename Q>
void test_receive_drains_before_close_impl()
{
    Q q;
    for (int i = 0; i < 3; ++i)
    {
        int v = i;
        q.send(std::move(v));
    }

    q.shutdown();

    int received = 0;
    try
    {
        while (true)
        {
            q.receive();
            ++received;
        }
    }
    catch (const QueueClosedException &) {}

    ASSERT_EQ(received, 3);
}

TEST(lifo_receive_drains_before_close)
{
    test_receive_drains_before_close_impl<MessageQueue<int>>();
}

TEST(fifo_receive_drains_before_close)
{
    test_receive_drains_before_close_impl<FifoMessageQueue<int>>();
}

template <typename Q>
void test_double_shutdown_impl()
{
    Q q;
    q.shutdown();
    q.shutdown();
    ASSERT_TRUE(q.is_closed());
}

TEST(lifo_double_shutdown_safe)
{
    test_double_shutdown_impl<MessageQueue<int>>();
}

TEST(fifo_double_shutdown_safe)
{
    test_double_shutdown_impl<FifoMessageQueue<int>>();
}

template <typename Q>
void test_shutdown_with_pending_receivers_impl()
{
    Q q;
    const int num_receivers = 4;
    std::atomic<int> unblocked{0};

    std::vector<std::thread> receivers;
    for (int i = 0; i < num_receivers; ++i)
    {
        receivers.emplace_back([&] {
            try { q.receive(); }
            catch (const QueueClosedException &) { unblocked.fetch_add(1); }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    q.shutdown();

    for (auto &t : receivers) t.join();
    ASSERT_EQ(unblocked.load(), num_receivers);
}

TEST(lifo_shutdown_unblocks_all_receivers)
{
    test_shutdown_with_pending_receivers_impl<MessageQueue<int>>();
}

TEST(fifo_shutdown_unblocks_all_receivers)
{
    test_shutdown_with_pending_receivers_impl<FifoMessageQueue<int>>();
}

TEST(lifo_empty_receive_on_closed_throws)
{
    MessageQueue<int> q;
    q.shutdown();
    ASSERT_THROWS(q.receive(), QueueClosedException);
}

TEST(fifo_empty_receive_on_closed_throws)
{
    FifoMessageQueue<int> q;
    q.shutdown();
    ASSERT_THROWS(q.receive(), QueueClosedException);
}

TEST(fifo_drain_ordering_under_shutdown)
{
    FifoMessageQueue<int> q;
    const int n = 10;
    for (int i = 0; i < n; ++i)
    {
        int v = i;
        q.send(std::move(v));
    }

    std::atomic<int> received{0};
    std::vector<int> order;
    std::mutex order_mtx;

    std::thread consumer([&] {
        try
        {
            while (true)
            {
                int val = q.receive();
                {
                    std::lock_guard<std::mutex> lk(order_mtx);
                    order.push_back(val);
                }
                received.fetch_add(1);
            }
        }
        catch (const QueueClosedException &) {}
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    q.shutdown();
    consumer.join();

    ASSERT_EQ(received.load(), n);
    ASSERT_EQ(static_cast<int>(order.size()), n);
    for (int i = 0; i < n; ++i)
    {
        ASSERT_EQ(order[i], i);
    }
}

TEST(concurrent_send_during_shutdown_is_safe)
{
    MessageQueue<int> q;
    const int num_senders = 4;
    std::atomic<int> send_ok{0};
    std::atomic<int> send_failed{0};

    std::vector<std::thread> senders;
    for (int s = 0; s < num_senders; ++s)
    {
        senders.emplace_back([&] {
            for (int i = 0; i < 50; ++i)
            {
                int v = i;
                try
                {
                    q.send(std::move(v));
                    send_ok.fetch_add(1);
                }
                catch (const QueueClosedException &)
                {
                    send_failed.fetch_add(1);
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    q.shutdown();

    for (auto &t : senders) t.join();

    ASSERT_TRUE(send_ok.load() > 0);
    ASSERT_TRUE(send_failed.load() > 0);
    ASSERT_EQ(send_ok.load() + send_failed.load(), num_senders * 50);
}

TEST(try_send_returns_false_after_shutdown)
{
    MessageQueue<int> q;
    q.shutdown();

    int v = 1;
    ASSERT_TRUE(!q.try_send(std::move(v)));

    FifoMessageQueue<int> fq;
    fq.shutdown();
    int v2 = 2;
    ASSERT_TRUE(!fq.try_send(std::move(v2)));
}

TEST(concurrent_try_send_during_shutdown)
{
    FifoMessageQueue<int> q;
    const int num_senders = 6;
    std::atomic<int> ok_count{0};
    std::atomic<int> fail_count{0};

    std::vector<std::thread> senders;
    for (int s = 0; s < num_senders; ++s)
    {
        senders.emplace_back([&] {
            for (int i = 0; i < 30; ++i)
            {
                int v = i;
                if (q.try_send(std::move(v)))
                    ok_count.fetch_add(1);
                else
                    fail_count.fetch_add(1);
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    q.shutdown();

    for (auto &t : senders) t.join();
    ASSERT_EQ(ok_count.load() + fail_count.load(), num_senders * 30);
    ASSERT_TRUE(ok_count.load() > 0);
}

int main()
{
    std::cout << "Queue hardening tests:\n";

    RUN(lifo_shutdown_unblocks_receiver);
    RUN(fifo_shutdown_unblocks_receiver);
    RUN(lifo_send_after_shutdown_throws);
    RUN(fifo_send_after_shutdown_throws);
    RUN(lifo_receive_drains_before_close);
    RUN(fifo_receive_drains_before_close);
    RUN(lifo_double_shutdown_safe);
    RUN(fifo_double_shutdown_safe);
    RUN(lifo_shutdown_unblocks_all_receivers);
    RUN(fifo_shutdown_unblocks_all_receivers);
    RUN(lifo_empty_receive_on_closed_throws);
    RUN(fifo_empty_receive_on_closed_throws);
    RUN(fifo_drain_ordering_under_shutdown);
    RUN(concurrent_send_during_shutdown_is_safe);
    RUN(try_send_returns_false_after_shutdown);
    RUN(concurrent_try_send_during_shutdown);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
