// benchmark_ucoro.cpp - performance benchmarks for ucoro C++23 wrapper
// because if you can't measure it, you can't brag about it
//
// compile with: g++ -std=c++23 -O3 -I../include -I../src -o benchmark_ucoro benchmark_ucoro.cpp

// macOS requires _XOPEN_SOURCE to expose deprecated ucontext functions
#if defined(__APPLE__)
#define _XOPEN_SOURCE 600
#endif

#define UCORO_IMPL
#include "ucoro/ucoro.hpp"

#include <algorithm>
#include <chrono>
#include <fmt/core.h>
#include <numeric>
#include <vector>

// --- Optional Dependencies ---

#ifdef HAVE_UCONTEXT
#include <ucontext.h>
#endif

#ifdef HAVE_BOOST_CONTEXT
#include <boost/context/fiber.hpp>
#endif

// ============================================================================
// timing utilities
// ============================================================================

class benchmark
{
public:
    using clock = std::chrono::high_resolution_clock;
    using duration = std::chrono::nanoseconds;

    struct result
    {
        std::string name;
        std::size_t iterations;
        duration total_time;
        duration min_time;
        duration max_time;
        duration mean_time;
        duration median_time;
        double ops_per_second;
    };

    template <typename F>
    [[nodiscard]] static auto run(
        std::string_view name,
        std::size_t iterations,
        F &&func) -> result
    {
        std::vector<duration> times;
        times.reserve(iterations);

        // warmup
        for (std::size_t i = 0; i < std::min(iterations / 10, std::size_t{100}); ++i)
        {
            func();
        }

        // actual benchmark
        for (std::size_t i = 0; i < iterations; ++i)
        {
            auto const start = clock::now();
            func();
            auto const end = clock::now();
            times.push_back(std::chrono::duration_cast<duration>(end - start));
        }

        // calculate statistics
        std::ranges::sort(times);

        auto const total = std::accumulate(times.begin(), times.end(), duration{});
        auto const mean = total / iterations;
        auto const median = times[iterations / 2];
        auto const min = times.front();
        auto const max = times.back();

        auto const seconds = std::chrono::duration<double>(total).count();
        auto const ops = static_cast<double>(iterations) / seconds;

        return {
            .name = std::string{name},
            .iterations = iterations,
            .total_time = total,
            .min_time = min,
            .max_time = max,
            .mean_time = mean,
            .median_time = median,
            .ops_per_second = ops};
    }

    static void print_result(result const &r)
    {
        fmt::println("┌─────────────────────────────────────────────────────────────");
        fmt::println("│ {}", r.name);
        fmt::println("├─────────────────────────────────────────────────────────────");
        fmt::println("│ iterations:   {:15}", r.iterations);
        fmt::println("│ total time:   {:15.3f} ms", std::chrono::duration<double, std::milli>(r.total_time).count());
        fmt::println("│ mean time:    {:15.1f} ns", static_cast<double>(r.mean_time.count()));
        fmt::println("│ median time:  {:15.1f} ns", static_cast<double>(r.median_time.count()));
        fmt::println("│ min time:     {:15.1f} ns", static_cast<double>(r.min_time.count()));
        fmt::println("│ max time:     {:15.1f} ns", static_cast<double>(r.max_time.count()));
        fmt::println("│ ops/sec:      {:15.0f}", r.ops_per_second);
        fmt::println("└─────────────────────────────────────────────────────────────\n");
    }
};

// ============================================================================
// Raw C Functions (for comparison)
// ============================================================================

void raw_noop(coro::detail::mco_coro *) {}

void raw_yield_loop(coro::detail::mco_coro *co)
{
    while (true)
    {
        coro::detail::mco_yield(co);
    }
}

void raw_storage_loop(coro::detail::mco_coro *co)
{
    int val;
    while (true)
    {
        coro::detail::mco_pop(co, &val, sizeof(int));
        coro::detail::mco_yield(co);
    }
}

// ============================================================================
// Benchmarks
// ============================================================================

void bench_create_destroy()
{
    // C++ Wrapper
    auto result = benchmark::run("coroutine create + destroy (C++ wrapper)", 100'000, []()
                                 { auto coro = coro::coroutine::create([](coro::coroutine_handle) {}); });
    benchmark::print_result(result);

    // Raw C API
    auto result_raw = benchmark::run("coroutine create + destroy (Raw C API)", 100'000, []()
                                     {
        coro::detail::mco_desc desc = coro::detail::mco_desc_init(raw_noop, 0);
        coro::detail::mco_coro* co = nullptr;
        coro::detail::mco_create(&co, &desc);
        coro::detail::mco_destroy(co); });
    benchmark::print_result(result_raw);
}

// ---------------------------------------------------------
// Context Switch Benchmarks (The Main Event)
// ---------------------------------------------------------

#ifdef HAVE_UCONTEXT
ucontext_t uctx_main, uctx_func;
char uctx_stack[1024 * 64];

void ucontext_func()
{
    while (true)
    {
        swapcontext(&uctx_func, &uctx_main);
    }
}
#endif

void bench_context_switch()
{
    // 1. ucoro (C++ Wrapper Safe)
    {
        auto coro_result = coro::coroutine::create([](coro::coroutine_handle h)
                                                   {
            while(true) {
                [[maybe_unused]] auto _ = h.yield();
            } });

        if (coro_result)
        {
            auto &coro = *coro_result;
            auto result = benchmark::run("context switch (ucoro C++ Safe)", 1'000'000, [&coro]()
                                         { [[maybe_unused]] auto _ = coro.resume(); });
            benchmark::print_result(result);
        }
    }

    // 1.5. ucoro (C++ Wrapper UNCHECKED)
    {
        auto coro_result = coro::coroutine::create([](coro::coroutine_handle h)
                                                   {
            while(true) {
                h.yield_unchecked(); // FAST PATH
            } });

        if (coro_result)
        {
            auto &coro = *coro_result;
            auto result = benchmark::run("context switch (ucoro C++ UNCHECKED)", 1'000'000, [&coro]()
                                         {
                                             coro.resume_unchecked(); // FAST PATH
                                         });
            benchmark::print_result(result);
        }
    }

    // 2. ucoro (Raw C API)
    {
        coro::detail::mco_desc desc = coro::detail::mco_desc_init(raw_yield_loop, 0);
        coro::detail::mco_coro *raw_co = nullptr;
        coro::detail::mco_create(&raw_co, &desc);

        auto result_raw = benchmark::run("context switch (ucoro Raw C API)", 1'000'000, [raw_co]()
                                         { coro::detail::mco_resume(raw_co); });
        benchmark::print_result(result_raw);
        coro::detail::mco_destroy(raw_co);
    }

    // 3. ucontext (POSIX)
#ifdef HAVE_UCONTEXT
    {
        getcontext(&uctx_func);
        uctx_func.uc_stack.ss_sp = uctx_stack;
        uctx_func.uc_stack.ss_size = sizeof(uctx_stack);
        uctx_func.uc_link = &uctx_main;
        makecontext(&uctx_func, ucontext_func, 0);

        auto result_uctx = benchmark::run("context switch (ucontext POSIX)", 1'000'000, []()
                                          { swapcontext(&uctx_main, &uctx_func); });
        benchmark::print_result(result_uctx);
    }
#else
    fmt::println("Skipping ucontext benchmark (not supported/enabled)");
#endif

    // 4. Boost.Context
#ifdef HAVE_BOOST_CONTEXT
    {
        namespace ctx = boost::context;
        // fixed-size stack allows fairer comparison to ucoro/ucontext default stacks
        ctx::fiber f = ctx::fiber(std::allocator_arg, ctx::fixedsize_stack(64 * 1024),
                                  [](ctx::fiber &&main)
                                  {
                                      while (true)
                                      {
                                          main = std::move(main).resume();
                                      }
                                      return std::move(main);
                                  });

        auto result_boost = benchmark::run("context switch (Boost.Context)", 1'000'000, [&f]()
                                           { f = std::move(f).resume(); });
        benchmark::print_result(result_boost);
    }
#else
    fmt::println("Skipping Boost.Context benchmark (library not found)");
#endif
}

void bench_storage_push_pop()
{
    // --- C++ Wrapper Setup (Safe) ---
    {
        auto coro_result = coro::coroutine::create([](coro::coroutine_handle h)
                                                   {
            while(true) {
                auto value = h.pop<int>();
                (void)value;
                [[maybe_unused]] auto _ = h.yield();
            } });

        if (coro_result)
        {
            auto &coro = *coro_result;
            auto result = benchmark::run("storage push + pop (C++ Safe)", 100'000, [&coro]()
                                         {
                [[maybe_unused]] auto _ = coro.push(42);
                [[maybe_unused]] auto __ = coro.resume(); });
            benchmark::print_result(result);
        }
    }

    // --- C++ Wrapper Setup (Unchecked) ---
    {
        auto coro_result = coro::coroutine::create([](coro::coroutine_handle h)
                                                   {
            while(true) {
                auto value = h.pop_unchecked<int>();
                (void)value;
                h.yield_unchecked();
            } });

        if (coro_result)
        {
            auto &coro = *coro_result;
            auto result = benchmark::run("storage push + pop (C++ Unchecked)", 100'000, [&coro]()
                                         {
                coro.push_unchecked(42);
                coro.resume_unchecked(); });
            benchmark::print_result(result);
        }
    }

    // --- Raw C API Setup ---
    {
        coro::detail::mco_desc desc = coro::detail::mco_desc_init(raw_storage_loop, 0);
        coro::detail::mco_coro *raw_co = nullptr;
        coro::detail::mco_create(&raw_co, &desc);

        auto result_raw = benchmark::run("storage push + pop (Raw C API)", 100'000, [raw_co]()
                                         {
            int val = 42;
            coro::detail::mco_push(raw_co, &val, sizeof(int));
            coro::detail::mco_resume(raw_co); });
        benchmark::print_result(result_raw);
        coro::detail::mco_destroy(raw_co);
    }
}

void bench_generator_iteration()
{
    auto gen_result = coro::generator<int>::create([](coro::coroutine_handle h)
                                                   {
        int i = 0;
        while(true) {
            [[maybe_unused]] auto _ = coro::yield_value(h, i++);
        } });

    if (!gen_result)
    {
        fmt::println(stderr, "failed to create generator for iteration benchmark");
        return;
    }

    auto &gen = *gen_result;

    auto result = benchmark::run("generator iteration (C++ only)", 100'000, [&gen]()
                                 { [[maybe_unused]] auto value = gen.next(); });
    benchmark::print_result(result);
}

void bench_memory_overhead()
{
    fmt::println("┌─────────────────────────────────────────────────────────────");
    fmt::println("│ memory overhead analysis");
    fmt::println("├─────────────────────────────────────────────────────────────");
    fmt::println("│ sizeof(mco_coro):          {:6} bytes", sizeof(coro::detail::mco_coro));
    fmt::println("│ sizeof(coro::coroutine):   {:6} bytes", sizeof(coro::coroutine));
    fmt::println("│ sizeof(coroutine_handle):  {:6} bytes", sizeof(coro::coroutine_handle));
    fmt::println("│ sizeof(coro::error):       {:6} bytes", sizeof(coro::error));
    fmt::println("│ sizeof(coro::state):       {:6} bytes", sizeof(coro::state));
    fmt::println("│ sizeof(coro::stack_size):  {:6} bytes", sizeof(coro::stack_size));
    fmt::println("│ sizeof(coro::task_runner): {:6} bytes", sizeof(coro::task_runner));
    fmt::println("│ default stack size:        {:6} bytes", coro::default_stack_size.value);
    fmt::println("│ default storage size:      {:6} bytes", coro::default_storage_size.value);
    fmt::println("└─────────────────────────────────────────────────────────────\n");
}

void bench_allocation_pattern()
{
    fmt::println("┌─────────────────────────────────────────────────────────────");
    fmt::println("│ allocation pattern analysis");
    fmt::println("├─────────────────────────────────────────────────────────────");

    constexpr std::size_t count = 1000;
    std::vector<coro::coroutine> coroutines;
    coroutines.reserve(count);

    auto const start = std::chrono::high_resolution_clock::now();

    for (std::size_t i = 0; i < count; ++i)
    {
        auto coro = coro::coroutine::create([](coro::coroutine_handle) {});
        if (coro)
        {
            coroutines.push_back(std::move(*coro));
        }
    }

    auto const after_create = std::chrono::high_resolution_clock::now();

    coroutines.clear();

    auto const after_destroy = std::chrono::high_resolution_clock::now();

    auto const create_time = std::chrono::duration<double, std::milli>(after_create - start).count();
    auto const destroy_time = std::chrono::duration<double, std::milli>(after_destroy - after_create).count();

    fmt::println("│ created {} coroutines in {:.2f} ms ({:.1f} ns each)",
                 count, create_time, create_time * 1e6 / static_cast<double>(count));
    fmt::println("│ destroyed {} coroutines in {:.2f} ms ({:.1f} ns each)",
                 count, destroy_time, destroy_time * 1e6 / static_cast<double>(count));
    fmt::println("└─────────────────────────────────────────────────────────────\n");
}

// ============================================================================
// main
// ============================================================================

int main()
{
    fmt::println("═══════════════════════════════════════════════════════════════");
    fmt::println("              ucoro C++23 wrapper benchmarks                ");
    fmt::println("═══════════════════════════════════════════════════════════════\n");

    bench_memory_overhead();
    bench_allocation_pattern();
    bench_create_destroy();
    bench_context_switch();
    bench_storage_push_pop();
    bench_generator_iteration();

    fmt::println("═══════════════════════════════════════════════════════════════");
    fmt::println("                     benchmarks complete                       ");
    fmt::println("═══════════════════════════════════════════════════════════════");

    return 0;
}