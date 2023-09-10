#include <iostream>
#include <atomic>
#include <thread>
#include <stdexcept>
#include <string>
#include "TrafficObject.h"
#include "WorkerState.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name()
#define RUN(name) do { \
    std::cout << "  " #name "..."; \
    try { name(); std::cout << " PASSED\n"; ++tests_passed; } \
    catch (const std::exception &e) { std::cout << " FAILED: " << e.what() << "\n"; ++tests_failed; } \
    catch (...) { std::cout << " FAILED (unknown)\n"; ++tests_failed; } \
} while(0)

#define ASSERT_TRUE(x) do { if (!(x)) throw std::runtime_error(std::string("ASSERT_TRUE failed at line ") + std::to_string(__LINE__)); } while(0)

// Observability helpers shared by the fixtures below. A "canary" member
// records when it has been destroyed, so a worker thread that keeps running
// past its owner's destruction can be caught touching freed memory.
namespace
{
std::atomic<bool> canary_destroyed{false};
std::atomic<bool> touched_after_destruction{false};
std::atomic<bool> worker_started{false};

void reset_observations()
{
    canary_destroyed.store(false);
    touched_after_destruction.store(false);
    worker_started.store(false);
}

struct Canary
{
    ~Canary() { canary_destroyed.store(true); }
    void touch() const
    {
        if (canary_destroyed.load())
            touched_after_destruction.store(true);
    }
};

// Mirrors how the production traffic objects (Vehicle, TrafficLight,
// Intersection) are built: a TrafficObject subclass owning members that its
// own worker thread reads.
class WorkerObject : public TrafficObject
{
public:
    ~WorkerObject()
    {
        shutdown();
        // The join must happen HERE, not in ~TrafficObject(): by the time the
        // base destructor runs, _canary and _workerState have already been
        // destroyed and a still-running work() would be touching freed memory.
        joinThreads();
    }

    void simulate() { threads.emplace_back(std::thread(&WorkerObject::work, this)); }
    void shutdown() { _workerState.stop(); }

private:
    void work()
    {
        worker_started.store(true);
        while (_workerState.is_running())
        {
            _canary.touch();
        }
        // Model a worker that is still mid-iteration when the stop is
        // signalled: it keeps touching its owner's members for a short while
        // after observing the stop, exactly as the production loops do before
        // reaching their next condition check.
        for (int i = 0; i < 200000; ++i)
        {
            _canary.touch();
        }
    }

    Canary _canary;
    WorkerState _workerState;
};
} // namespace

TEST(test_destructor_joins_before_members_are_destroyed)
{
    // Regression test: the derived destructors used to only *signal* shutdown
    // and leave the join to ~TrafficObject(). Because a derived class's
    // members are destroyed after its own destructor body but before the base
    // destructor runs, the worker thread outlived the very members it reads
    // (WorkerState's mutex/condition variable, message queues, shared_ptr
    // members) -- a use-after-free on every teardown of a running object.
    reset_observations();
    {
        WorkerObject obj;
        obj.simulate();
        while (!worker_started.load())
            std::this_thread::yield();
    }

    ASSERT_TRUE(canary_destroyed.load());
    ASSERT_TRUE(!touched_after_destruction.load());
}

TEST(test_teardown_is_clean_under_repetition)
{
    // The window between "members destroyed" and "base class joins" is small,
    // so repeat the teardown to make any regression show up reliably.
    for (int i = 0; i < 50; ++i)
    {
        reset_observations();
        {
            WorkerObject obj;
            obj.simulate();
            while (!worker_started.load())
                std::this_thread::yield();
        }
        ASSERT_TRUE(!touched_after_destruction.load());
    }
}

TEST(test_join_threads_is_idempotent)
{
    // ~TrafficObject() still calls joinThreads() as a safety net, so it runs a
    // second time after the derived destructor already joined. That must be a
    // no-op rather than a std::system_error on a non-joinable thread.
    reset_observations();
    {
        WorkerObject obj;
        obj.simulate();
        while (!worker_started.load())
            std::this_thread::yield();
    }
    ASSERT_TRUE(true); // reaching here without an exception is the assertion
}

TEST(test_object_without_threads_destroys_cleanly)
{
    WorkerObject obj; // never simulated: no threads to join
    ASSERT_TRUE(obj.getID() >= 0);
}

int main()
{
    std::cout << "TrafficObject teardown tests:\n";

    RUN(test_destructor_joins_before_members_are_destroyed);
    RUN(test_teardown_is_clean_under_repetition);
    RUN(test_join_threads_is_idempotent);
    RUN(test_object_without_threads_destroys_cleanly);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
