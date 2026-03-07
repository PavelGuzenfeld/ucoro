// test_integration.cpp - end-to-end integration tests for ucoro
// Tests real-world scenarios combining multiple features.

#include <fmt/core.h>

#define UCORO_IMPL
#include "ucoro/ucoro.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Simple test framework (no doctest dependency for integration tests)
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(expr)                                                    \
    do                                                                       \
    {                                                                        \
        if (!(expr))                                                         \
        {                                                                    \
            fmt::println(stderr, "  FAIL: {} (line {})", #expr, __LINE__);   \
            tests_failed++;                                                  \
            return;                                                          \
        }                                                                    \
    } while (0)

#define ASSERT_EQ(a, b)                                                              \
    do                                                                               \
    {                                                                                \
        if ((a) != (b))                                                              \
        {                                                                            \
            fmt::println(stderr, "  FAIL: {} != {} (line {})", #a, #b, __LINE__);    \
            tests_failed++;                                                          \
            return;                                                                  \
        }                                                                            \
    } while (0)

#define RUN_TEST(fn)                                        \
    do                                                      \
    {                                                       \
        fmt::print("  {:.<60}", #fn);                       \
        fn();                                               \
        if (tests_failed == prev_failed)                    \
        {                                                   \
            fmt::println(" PASS");                          \
            tests_passed++;                                 \
        }                                                   \
        prev_failed = tests_failed;                         \
    } while (0)

// ============================================================================
// Integration Test: Producer-Consumer Pipeline
// ============================================================================

void test_producer_consumer_pipeline()
{
    // Producer generates values, consumer processes them, results collected
    std::vector<int> results;

    auto producer = coro::generator<int>::create([](coro::coroutine_handle h)
                                                  {
        for (int i = 1; i <= 10; ++i)
        {
            [[maybe_unused]] auto _ = coro::yield_value(h, i * i);
        } });

    ASSERT_TRUE(producer.has_value());

    for (auto const &value : *producer)
    {
        results.push_back(value);
    }

    ASSERT_EQ(results.size(), 10u);
    ASSERT_EQ(results[0], 1);
    ASSERT_EQ(results[4], 25);
    ASSERT_EQ(results[9], 100);

    int sum = std::accumulate(results.begin(), results.end(), 0);
    ASSERT_EQ(sum, 385); // sum of squares 1..10
}

// ============================================================================
// Integration Test: Cooperative Task Scheduling
// ============================================================================

void test_cooperative_scheduling()
{
    // Simulate 3 independent tasks doing "work" in round-robin
    std::vector<std::string> log;
    coro::task_runner runner;

    auto make_task = [&log](std::string name, int steps) -> coro::coroutine
    {
        auto result = coro::coroutine::create([&log, name = std::move(name), steps](coro::coroutine_handle h)
                                               {
            for (int i = 0; i < steps; ++i)
            {
                log.push_back(name + ":" + std::to_string(i));
                if (i < steps - 1)
                    [[maybe_unused]] auto _ = h.yield();
            } });
        return std::move(*result);
    };

    runner.add(make_task("A", 3));
    runner.add(make_task("B", 2));
    runner.add(make_task("C", 4));

    auto run_result = runner.run();
    ASSERT_TRUE(run_result.has_value());
    ASSERT_TRUE(runner.empty());

    // Verify interleaving: A, B, C should alternate
    ASSERT_EQ(log.size(), 9u);

    // First round: all three run
    ASSERT_EQ(log[0], "A:0");
    ASSERT_EQ(log[1], "B:0");
    ASSERT_EQ(log[2], "C:0");

    // Second round: A, B, C (B finishes)
    ASSERT_EQ(log[3], "A:1");
    ASSERT_EQ(log[4], "B:1");
    ASSERT_EQ(log[5], "C:1");

    // Third round: A finishes, C continues
    ASSERT_EQ(log[6], "A:2");
    ASSERT_EQ(log[7], "C:2");

    // Fourth round: only C
    ASSERT_EQ(log[8], "C:3");
}

// ============================================================================
// Integration Test: Data Passing Ping-Pong
// ============================================================================

void test_data_passing_ping_pong()
{
    // Coroutine receives a value, doubles it, sends back, repeat
    auto result = coro::coroutine::create([](coro::coroutine_handle h)
                                           {
        for (int i = 0; i < 5; ++i)
        {
            auto val = h.pop<int>();
            if (val)
            {
                h.push_unchecked(*val * 2);
            }
            if (i < 4)
                h.yield_unchecked();
        } });

    ASSERT_TRUE(result.has_value());
    auto &coro = *result;

    int value = 1;
    for (int i = 0; i < 5; ++i)
    {
        (void)coro.push(value);
        (void)coro.resume();
        auto popped = coro.pop<int>();
        ASSERT_TRUE(popped.has_value());
        value = *popped;
    }

    // 1 -> 2 -> 4 -> 8 -> 16 -> 32
    ASSERT_EQ(value, 32);
}

// ============================================================================
// Integration Test: Deep Yield (Stackful Advantage)
// ============================================================================

static void recursive_yield(coro::coroutine_handle h, int depth, int max_depth, std::vector<int> &trace)
{
    trace.push_back(depth);
    if (depth < max_depth)
    {
        // Yield from deep in the call stack — impossible with stackless coroutines
        [[maybe_unused]] auto _ = h.yield();
        recursive_yield(h, depth + 1, max_depth, trace);
    }
}

void test_deep_yield()
{
    std::vector<int> trace;
    auto result = coro::coroutine::create([&trace](coro::coroutine_handle h)
                                           { recursive_yield(h, 0, 5, trace); });

    ASSERT_TRUE(result.has_value());
    auto &coro = *result;

    while (!coro.done())
    {
        (void)coro.resume();
    }

    // Should have depths 0, 1, 2, 3, 4, 5
    ASSERT_EQ(trace.size(), 6u);
    for (std::size_t i = 0; i < trace.size(); ++i)
    {
        ASSERT_EQ(trace[i], static_cast<int>(i));
    }
}

// ============================================================================
// Integration Test: Generator Chaining
// ============================================================================

void test_generator_chaining()
{
    // Generate fibonacci, filter evens, take first 5
    auto fib_gen = coro::generator<int>::create([](coro::coroutine_handle h)
                                                 {
        int a = 0, b = 1;
        while (true)
        {
            [[maybe_unused]] auto _ = coro::yield_value(h, a);
            int next = a + b;
            a = b;
            b = next;
        } });

    ASSERT_TRUE(fib_gen.has_value());

    std::vector<int> even_fibs;
    for (auto const &value : *fib_gen)
    {
        if (value % 2 == 0)
        {
            even_fibs.push_back(value);
            if (even_fibs.size() >= 5)
                break;
        }
    }

    ASSERT_EQ(even_fibs.size(), 5u);
    // Even Fibonacci numbers: 0, 2, 8, 34, 144
    ASSERT_EQ(even_fibs[0], 0);
    ASSERT_EQ(even_fibs[1], 2);
    ASSERT_EQ(even_fibs[2], 8);
    ASSERT_EQ(even_fibs[3], 34);
    ASSERT_EQ(even_fibs[4], 144);
}

// ============================================================================
// Integration Test: Exception Propagation Through Yields
// ============================================================================

void test_exception_after_work()
{
    // Coroutine does real work, then fails
    std::vector<int> processed;
    auto result = coro::coroutine::create([&processed](coro::coroutine_handle h)
                                           {
        for (int i = 0; i < 3; ++i)
        {
            processed.push_back(i * 10);
            [[maybe_unused]] auto _ = h.yield();
        }
        throw std::runtime_error("processing failed at item 3"); });

    ASSERT_TRUE(result.has_value());
    auto &coro = *result;

    // First 3 resumes should work
    for (int i = 0; i < 3; ++i)
    {
        auto r = coro.resume();
        ASSERT_TRUE(r.has_value());
        ASSERT_TRUE(!coro.has_exception());
    }

    // 4th resume: coroutine throws
    auto r = coro.resume();
    ASSERT_TRUE(r.has_value()); // resume itself succeeds
    ASSERT_TRUE(coro.done());
    ASSERT_TRUE(coro.has_exception());

    // Work before exception was captured
    ASSERT_EQ(processed.size(), 3u);
    ASSERT_EQ(processed[0], 0);
    ASSERT_EQ(processed[1], 10);
    ASSERT_EQ(processed[2], 20);

    // Exception is retrievable
    bool caught = false;
    try
    {
        coro.rethrow_if_exception();
    }
    catch (std::runtime_error const &e)
    {
        caught = true;
        ASSERT_TRUE(std::string_view{e.what()} == "processing failed at item 3");
    }
    ASSERT_TRUE(caught);
}

// ============================================================================
// Integration Test: Multi-threaded Independence
// ============================================================================

void test_multithread_independence()
{
    constexpr int num_threads = 4;
    constexpr int iterations = 1000;

    std::array<int, num_threads> results{};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([&results, t]()
                              {
            int sum = 0;
            for (int i = 0; i < iterations; ++i)
            {
                auto coro = coro::coroutine::create([&sum, t](coro::coroutine_handle h)
                {
                    sum += (t + 1);
                    h.yield_unchecked();
                    sum += (t + 1);
                });
                if (coro)
                {
                    coro->resume_unchecked();
                    coro->resume_unchecked();
                }
            }
            results[static_cast<std::size_t>(t)] = sum; });
    }

    for (auto &t : threads)
        t.join();

    // Each thread should have sum = (t+1) * 2 * iterations
    for (int t = 0; t < num_threads; ++t)
    {
        int expected = (t + 1) * 2 * iterations;
        ASSERT_EQ(results[static_cast<std::size_t>(t)], expected);
    }
}

// ============================================================================
// Integration Test: Step-by-Step Task Runner
// ============================================================================

void test_task_runner_step()
{
    // Use step() for frame-by-frame game-like updates
    std::vector<int> frames;
    coro::task_runner runner;

    auto task = coro::coroutine::create([&frames](coro::coroutine_handle h)
                                         {
        for (int frame = 0; frame < 5; ++frame)
        {
            frames.push_back(frame);
            if (frame < 4)
                [[maybe_unused]] auto _ = h.yield();
        } });

    ASSERT_TRUE(task.has_value());
    runner.add(std::move(*task));

    int step_count = 0;
    while (true)
    {
        auto result = runner.step();
        ASSERT_TRUE(result.has_value());
        step_count++;
        if (!*result) // no more tasks
            break;
    }

    ASSERT_EQ(step_count, 5);
    ASSERT_EQ(frames.size(), 5u);
}

// ============================================================================
// Integration Test: Large Struct Transfer
// ============================================================================

void test_large_struct_transfer()
{
    struct SensorData
    {
        std::array<float, 64> readings;
        int timestamp;
        int sensor_id;
    };

    SensorData received{};
    auto result = coro::coroutine::create(
        [&received](coro::coroutine_handle h)
        {
            auto data = h.pop<SensorData>();
            if (data)
            {
                // Process: scale all readings
                received = *data;
                for (auto &r : received.readings)
                    r *= 2.0f;
            }
        },
        coro::default_stack_size,
        coro::storage_size{2048});

    ASSERT_TRUE(result.has_value());

    SensorData sent{};
    for (std::size_t i = 0; i < sent.readings.size(); ++i)
        sent.readings[i] = static_cast<float>(i);
    sent.timestamp = 12345;
    sent.sensor_id = 42;

    (void)result->push(sent);
    (void)result->resume();

    ASSERT_EQ(received.timestamp, 12345);
    ASSERT_EQ(received.sensor_id, 42);
    for (std::size_t i = 0; i < 64; ++i)
    {
        float expected = static_cast<float>(i) * 2.0f;
        ASSERT_TRUE(std::abs(received.readings[i] - expected) < 0.001f);
    }
}

// ============================================================================
// Integration Test: Coroutine Move Semantics Under Stress
// ============================================================================

void test_move_semantics_stress()
{
    // Create, move, store in vector, run — exercises RAII
    std::vector<coro::coroutine> coroutines;
    std::atomic<int> counter{0};

    for (int i = 0; i < 100; ++i)
    {
        auto result = coro::coroutine::create([&counter](coro::coroutine_handle h)
                                               {
            counter.fetch_add(1);
            [[maybe_unused]] auto _ = h.yield();
            counter.fetch_add(1); });
        ASSERT_TRUE(result.has_value());
        coroutines.push_back(std::move(*result));
    }

    // Resume all once
    for (auto &c : coroutines)
    {
        auto r = c.resume();
        ASSERT_TRUE(r.has_value());
    }
    ASSERT_EQ(counter.load(), 100);

    // Move half into a new vector
    std::vector<coro::coroutine> moved;
    for (std::size_t i = 0; i < 50; ++i)
    {
        moved.push_back(std::move(coroutines[i]));
    }

    // Resume moved ones
    for (auto &c : moved)
    {
        auto r = c.resume();
        ASSERT_TRUE(r.has_value());
    }
    ASSERT_EQ(counter.load(), 150);

    // Original moved-from coroutines should be invalid
    for (std::size_t i = 0; i < 50; ++i)
    {
        ASSERT_TRUE(!coroutines[i].valid());
    }

    // Remaining ones still valid
    for (std::size_t i = 50; i < 100; ++i)
    {
        ASSERT_TRUE(coroutines[i].valid());
        (void)coroutines[i].resume();
    }
    ASSERT_EQ(counter.load(), 200);
}

// ============================================================================
// Integration Test: Guard Page Verification
// ============================================================================

void test_guard_page_mmap_allocation()
{
    // Verify coroutines with guard pages can be created and used normally
    // (Guard pages use mmap instead of calloc — verify no regression)
    constexpr int count = 50;
    std::vector<coro::coroutine> coroutines;

    for (int i = 0; i < count; ++i)
    {
        auto result = coro::coroutine::create(
            [i](coro::coroutine_handle h)
            {
                volatile int x = i * i; // use stack
                (void)x;
                [[maybe_unused]] auto _ = h.yield();
                volatile int y = i * i * i;
                (void)y;
            },
            coro::stack_size{64 * 1024});
        ASSERT_TRUE(result.has_value());
        coroutines.push_back(std::move(*result));
    }

    // Resume all twice (yield + finish)
    for (auto &c : coroutines)
    {
        (void)c.resume();
        ASSERT_TRUE(c.suspended());
    }
    for (auto &c : coroutines)
    {
        (void)c.resume();
        ASSERT_TRUE(c.done());
    }

    // Destructor should cleanly munmap all allocations
    coroutines.clear();
}

// ============================================================================
// Performance Test: Context Switch Latency
// ============================================================================

void test_perf_context_switch()
{
    auto coro_result = coro::coroutine::create([](coro::coroutine_handle h)
                                                {
        while (true)
            h.yield_unchecked(); });

    ASSERT_TRUE(coro_result.has_value());
    auto &coro = *coro_result;

    constexpr int warmup = 10000;
    constexpr int iterations = 1000000;

    for (int i = 0; i < warmup; ++i)
        coro.resume_unchecked();

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
        coro.resume_unchecked();
    auto end = std::chrono::high_resolution_clock::now();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double ns_per_switch = static_cast<double>(ns) / iterations;
    double ops_per_sec = 1e9 / ns_per_switch;

    fmt::println("");
    fmt::println("    Context switch (unchecked): {:.1f} ns/op, {:.1f}M ops/sec", ns_per_switch, ops_per_sec / 1e6);
    ASSERT_TRUE(ns_per_switch < 200.0); // should be well under 200ns
}

void test_perf_context_switch_safe()
{
    auto coro_result = coro::coroutine::create([](coro::coroutine_handle h)
                                                {
        while (true)
            [[maybe_unused]] auto _ = h.yield(); });

    ASSERT_TRUE(coro_result.has_value());
    auto &coro = *coro_result;

    constexpr int warmup = 10000;
    constexpr int iterations = 1000000;

    for (int i = 0; i < warmup; ++i)
        (void)coro.resume();

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
        (void)coro.resume();
    auto end = std::chrono::high_resolution_clock::now();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double ns_per_switch = static_cast<double>(ns) / iterations;
    double ops_per_sec = 1e9 / ns_per_switch;

    fmt::println("");
    fmt::println("    Context switch (safe):      {:.1f} ns/op, {:.1f}M ops/sec", ns_per_switch, ops_per_sec / 1e6);
    ASSERT_TRUE(ns_per_switch < 2000.0); // generous limit (ASan adds ~10x overhead in Debug)
}

void test_perf_create_destroy()
{
    constexpr int iterations = 10000;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
    {
        auto coro = coro::coroutine::create([](coro::coroutine_handle) {});
        (void)coro; // RAII destroy
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double ns_per_op = static_cast<double>(ns) / iterations;

    fmt::println("");
    fmt::println("    Create+destroy: {:.0f} ns/op", ns_per_op);
    ASSERT_TRUE(ns_per_op < 100000.0); // should be well under 100us
}

void test_perf_generator_throughput()
{
    auto gen = coro::generator<int>::create([](coro::coroutine_handle h)
                                             {
        int i = 0;
        while (true)
            [[maybe_unused]] auto _ = coro::yield_value(h, i++); });

    ASSERT_TRUE(gen.has_value());

    constexpr int warmup = 10000;
    constexpr int iterations = 1000000;

    for (int i = 0; i < warmup; ++i)
        (void)gen->next();

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
        (void)gen->next();
    auto end = std::chrono::high_resolution_clock::now();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double ns_per_op = static_cast<double>(ns) / iterations;
    double ops_per_sec = 1e9 / ns_per_op;

    fmt::println("");
    fmt::println("    Generator iteration: {:.1f} ns/op, {:.1f}M ops/sec", ns_per_op, ops_per_sec / 1e6);
    ASSERT_TRUE(ns_per_op < 2000.0); // generous limit (ASan adds ~10x overhead in Debug)
}

void test_perf_storage_throughput()
{
    auto coro_result = coro::coroutine::create([](coro::coroutine_handle h)
                                                {
        while (true)
        {
            int val = h.pop_unchecked<int>();
            h.push_unchecked(val * 2);
            h.yield_unchecked();
        } });

    ASSERT_TRUE(coro_result.has_value());
    auto &coro = *coro_result;

    constexpr int warmup = 10000;
    constexpr int iterations = 1000000;

    for (int i = 0; i < warmup; ++i)
    {
        coro.push_unchecked(42);
        coro.resume_unchecked();
        (void)coro.pop_unchecked<int>();
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
    {
        coro.push_unchecked(42);
        coro.resume_unchecked();
        (void)coro.pop_unchecked<int>();
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double ns_per_op = static_cast<double>(ns) / iterations;

    fmt::println("");
    fmt::println("    Push+switch+pop (unchecked): {:.1f} ns/op", ns_per_op);
    ASSERT_TRUE(ns_per_op < 2000.0); // generous limit (ASan adds ~10x overhead in Debug)
}

// ============================================================================
// Integration Test: vs ucontext comparison
// ============================================================================

#ifdef __unix__
#include <ucontext.h>

static ucontext_t uctx_main_it, uctx_coro_it;
static char uctx_stack_it[64 * 1024];

static void ucontext_yield_loop()
{
    while (true)
        swapcontext(&uctx_coro_it, &uctx_main_it);
}

void test_perf_vs_ucontext()
{
    getcontext(&uctx_coro_it);
    uctx_coro_it.uc_stack.ss_sp = uctx_stack_it;
    uctx_coro_it.uc_stack.ss_size = sizeof(uctx_stack_it);
    uctx_coro_it.uc_link = &uctx_main_it;
    makecontext(&uctx_coro_it, ucontext_yield_loop, 0);

    constexpr int iterations = 1000000;

    // Warmup
    for (int i = 0; i < 10000; ++i)
        swapcontext(&uctx_main_it, &uctx_coro_it);

    auto start_uctx = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
        swapcontext(&uctx_main_it, &uctx_coro_it);
    auto end_uctx = std::chrono::high_resolution_clock::now();

    auto ns_uctx = std::chrono::duration_cast<std::chrono::nanoseconds>(end_uctx - start_uctx).count();
    double ns_per_uctx = static_cast<double>(ns_uctx) / iterations;

    // ucoro unchecked
    auto coro_result = coro::coroutine::create([](coro::coroutine_handle h)
                                                {
        while (true)
            h.yield_unchecked(); });
    auto &coro = *coro_result;

    for (int i = 0; i < 10000; ++i)
        coro.resume_unchecked();

    auto start_ucoro = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
        coro.resume_unchecked();
    auto end_ucoro = std::chrono::high_resolution_clock::now();

    auto ns_ucoro = std::chrono::duration_cast<std::chrono::nanoseconds>(end_ucoro - start_ucoro).count();
    double ns_per_ucoro = static_cast<double>(ns_ucoro) / iterations;

    double speedup = ns_per_uctx / ns_per_ucoro;

    fmt::println("");
    fmt::println("    ucontext:        {:.1f} ns/op", ns_per_uctx);
    fmt::println("    ucoro unchecked: {:.1f} ns/op", ns_per_ucoro);
    fmt::println("    speedup:         {:.1f}x", speedup);

    ASSERT_TRUE(speedup > 2.0); // must be at least 2x faster
}
#endif

// ============================================================================
// Main
// ============================================================================

int main()
{
    int prev_failed = 0;

    fmt::println("=== Integration Tests ===");
    fmt::println("");

    fmt::println("[Functional]");
    RUN_TEST(test_producer_consumer_pipeline);
    RUN_TEST(test_cooperative_scheduling);
    RUN_TEST(test_data_passing_ping_pong);
    RUN_TEST(test_deep_yield);
    RUN_TEST(test_generator_chaining);
    RUN_TEST(test_exception_after_work);
    RUN_TEST(test_multithread_independence);
    RUN_TEST(test_task_runner_step);
    RUN_TEST(test_large_struct_transfer);
    RUN_TEST(test_move_semantics_stress);
    RUN_TEST(test_guard_page_mmap_allocation);

    fmt::println("");
    fmt::println("[Performance]");
    RUN_TEST(test_perf_context_switch);
    RUN_TEST(test_perf_context_switch_safe);
    RUN_TEST(test_perf_create_destroy);
    RUN_TEST(test_perf_generator_throughput);
    RUN_TEST(test_perf_storage_throughput);
#ifdef __unix__
    RUN_TEST(test_perf_vs_ucontext);
#endif

    fmt::println("");
    fmt::println("=== Results: {} passed, {} failed ===", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
