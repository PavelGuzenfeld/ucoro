// benchmark_ucoro.cpp - performance benchmarks for ucoro C++23 wrapper
// because if you can't measure it, you can't brag about it
//
// compile with: g++ -std=c++23 -O3 -I../include -I../src -o benchmark_ucoro benchmark_ucoro.cpp

#define UCORO_IMPL
#include "ucoro/ucoro.hpp"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <print>
#include <vector>

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
        std::println("┌─────────────────────────────────────────────────────────────");
        std::println("│ {}", r.name);
        std::println("├─────────────────────────────────────────────────────────────");
        std::println("│ iterations:   {:15}", r.iterations);
        std::println("│ total time:   {:15.3f} ms", std::chrono::duration<double, std::milli>(r.total_time).count());
        std::println("│ mean time:    {:15.1f} ns", static_cast<double>(r.mean_time.count()));
        std::println("│ median time:  {:15.1f} ns", static_cast<double>(r.median_time.count()));
        std::println("│ min time:     {:15.1f} ns", static_cast<double>(r.min_time.count()));
        std::println("│ max time:     {:15.1f} ns", static_cast<double>(r.max_time.count()));
        std::println("│ ops/sec:      {:15.0f}", r.ops_per_second);
        std::println("└─────────────────────────────────────────────────────────────\n");
    }
};

// ============================================================================
// benchmarks
// ============================================================================

void bench_create_destroy()
{
    auto result = benchmark::run("coroutine create + destroy", 100'000, []()
                                 {
                                     auto coro = coro::coroutine::create([](coro::coroutine_handle) {});
                                     // destructor runs here
                                 });
    benchmark::print_result(result);
}

void bench_context_switch()
{
    // create a coroutine that yields many times
    auto coro_result = coro::coroutine::create([](coro::coroutine_handle h)
                                               {
        for (int i = 0; i < 1'000'000; ++i) {
            [[maybe_unused]] auto _ = h.yield();
        } });

    if (!coro_result)
    {
        std::println(stderr, "failed to create coroutine for context switch benchmark");
        return;
    }

    auto &coro = *coro_result;

    auto result = benchmark::run("context switch (yield + resume)", 1'000'000, [&coro]()
                                 { [[maybe_unused]] auto _ = coro.resume(); });
    benchmark::print_result(result);
}

void bench_storage_push_pop()
{
    auto coro_result = coro::coroutine::create([](coro::coroutine_handle h)
                                               {
        for (int i = 0; i < 100'000; ++i) {
            auto value = h.pop<int>();
            (void)value;
            [[maybe_unused]] auto _ = h.yield();
        } });

    if (!coro_result)
    {
        std::println(stderr, "failed to create coroutine for storage benchmark");
        return;
    }

    auto &coro = *coro_result;

    auto result = benchmark::run("storage push + pop (int)", 100'000, [&coro]()
                                 {
        [[maybe_unused]] auto _ = coro.push(42);
        [[maybe_unused]] auto __ = coro.resume(); });
    benchmark::print_result(result);
}

void bench_generator_iteration()
{
    auto gen_result = coro::generator<int>::create([](coro::coroutine_handle h)
                                                   {
        for (int i = 0; i < 100'000; ++i) {
            [[maybe_unused]] auto _ = coro::yield_value(h, i);
        } });

    if (!gen_result)
    {
        std::println(stderr, "failed to create generator for iteration benchmark");
        return;
    }

    auto &gen = *gen_result;

    auto result = benchmark::run("generator iteration", 100'000, [&gen]()
                                 { [[maybe_unused]] auto value = gen.next(); });
    benchmark::print_result(result);
}

void bench_memory_overhead()
{
    std::println("┌─────────────────────────────────────────────────────────────");
    std::println("│ memory overhead analysis");
    std::println("├─────────────────────────────────────────────────────────────");
    std::println("│ sizeof(mco_coro):          {:6} bytes", sizeof(coro::detail::mco_coro));
    std::println("│ sizeof(coro::coroutine):   {:6} bytes", sizeof(coro::coroutine));
    std::println("│ sizeof(coroutine_handle):  {:6} bytes", sizeof(coro::coroutine_handle));
    std::println("│ sizeof(coro::error):       {:6} bytes", sizeof(coro::error));
    std::println("│ sizeof(coro::state):       {:6} bytes", sizeof(coro::state));
    std::println("│ sizeof(coro::stack_size):  {:6} bytes", sizeof(coro::stack_size));
    std::println("│ sizeof(coro::task_runner): {:6} bytes", sizeof(coro::task_runner));
    std::println("│ default stack size:        {:6} bytes", coro::default_stack_size.value);
    std::println("│ default storage size:      {:6} bytes", coro::default_storage_size.value);
    std::println("└─────────────────────────────────────────────────────────────\n");
}

void bench_allocation_pattern()
{
    std::println("┌─────────────────────────────────────────────────────────────");
    std::println("│ allocation pattern analysis");
    std::println("├─────────────────────────────────────────────────────────────");

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

    std::println("│ created {} coroutines in {:.2f} ms ({:.1f} ns each)",
                 count, create_time, create_time * 1e6 / static_cast<double>(count));
    std::println("│ destroyed {} coroutines in {:.2f} ms ({:.1f} ns each)",
                 count, destroy_time, destroy_time * 1e6 / static_cast<double>(count));
    std::println("└─────────────────────────────────────────────────────────────\n");
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::println("═══════════════════════════════════════════════════════════════");
    std::println("              ucoro C++23 wrapper benchmarks                ");
    std::println("═══════════════════════════════════════════════════════════════\n");

    bench_memory_overhead();
    bench_allocation_pattern();
    bench_create_destroy();
    bench_context_switch();
    bench_storage_push_pop();
    bench_generator_iteration();

    std::println("═══════════════════════════════════════════════════════════════");
    std::println("                     benchmarks complete                       ");
    std::println("═══════════════════════════════════════════════════════════════");

    return 0;
}