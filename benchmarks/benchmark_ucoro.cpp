// benchmark_ucoro.cpp - performance benchmarks for ucoro C++23 wrapper
// because if you can't measure it, you can't brag about it
//
// compile with: g++ -std=c++23 -O3 -I../include -I../src -o benchmark_ucoro benchmark_ucoro.cpp

#define UCORO_IMPL
#include "ucoro/impl/minicoro_impl.h"
#include "ucoro/ucoro.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <iomanip>
#include <iostream>
#include <numeric>
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
        std::cout << "┌─────────────────────────────────────────────────────────────\n";
        std::cout << "│ " << r.name << "\n";
        std::cout << "├─────────────────────────────────────────────────────────────\n";
        std::cout << "│ iterations:   " << std::setw(15) << r.iterations << "\n";
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "│ total time:   " << std::setw(15)
                  << std::chrono::duration<double, std::milli>(r.total_time).count() << " ms\n";
        std::cout << std::setprecision(1);
        std::cout << "│ mean time:    " << std::setw(15)
                  << static_cast<double>(r.mean_time.count()) << " ns\n";
        std::cout << "│ median time:  " << std::setw(15)
                  << static_cast<double>(r.median_time.count()) << " ns\n";
        std::cout << "│ min time:     " << std::setw(15)
                  << static_cast<double>(r.min_time.count()) << " ns\n";
        std::cout << "│ max time:     " << std::setw(15)
                  << static_cast<double>(r.max_time.count()) << " ns\n";
        std::cout << std::setprecision(0);
        std::cout << "│ ops/sec:      " << std::setw(15) << r.ops_per_second << "\n";
        std::cout << "└─────────────────────────────────────────────────────────────\n\n";
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
        std::cerr << "failed to create coroutine for context switch benchmark\n";
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
        std::cerr << "failed to create coroutine for storage benchmark\n";
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
        std::cerr << "failed to create generator for iteration benchmark\n";
        return;
    }

    auto &gen = *gen_result;

    auto result = benchmark::run("generator iteration", 100'000, [&gen]()
                                 { [[maybe_unused]] auto value = gen.next(); });
    benchmark::print_result(result);
}

void bench_memory_overhead()
{
    std::cout << "┌─────────────────────────────────────────────────────────────\n";
    std::cout << "│ memory overhead analysis\n";
    std::cout << "├─────────────────────────────────────────────────────────────\n";
    std::cout << "│ sizeof(mco_coro):          " << std::setw(6) << sizeof(mco_coro) << " bytes\n";
    std::cout << "│ sizeof(coro::coroutine):   " << std::setw(6) << sizeof(coro::coroutine) << " bytes\n";
    std::cout << "│ sizeof(coroutine_handle):  " << std::setw(6) << sizeof(coro::coroutine_handle) << " bytes\n";
    std::cout << "│ sizeof(coro::error):       " << std::setw(6) << sizeof(coro::error) << " bytes\n";
    std::cout << "│ sizeof(coro::state):       " << std::setw(6) << sizeof(coro::state) << " bytes\n";
    std::cout << "│ sizeof(coro::stack_size):  " << std::setw(6) << sizeof(coro::stack_size) << " bytes\n";
    std::cout << "│ sizeof(coro::task_runner): " << std::setw(6) << sizeof(coro::task_runner) << " bytes\n";
    std::cout << "│ default stack size:        " << std::setw(6) << coro::default_stack_size.value << " bytes\n";
    std::cout << "│ default storage size:      " << std::setw(6) << coro::default_storage_size.value << " bytes\n";
    std::cout << "└─────────────────────────────────────────────────────────────\n\n";
}

void bench_allocation_pattern()
{
    std::cout << "┌─────────────────────────────────────────────────────────────\n";
    std::cout << "│ allocation pattern analysis\n";
    std::cout << "├─────────────────────────────────────────────────────────────\n";

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

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "│ created " << count << " coroutines in " << create_time << " ms ("
              << std::setprecision(1) << create_time * 1e6 / static_cast<double>(count) << " ns each)\n";
    std::cout << std::setprecision(2);
    std::cout << "│ destroyed " << count << " coroutines in " << destroy_time << " ms ("
              << std::setprecision(1) << destroy_time * 1e6 / static_cast<double>(count) << " ns each)\n";
    std::cout << "└─────────────────────────────────────────────────────────────\n\n";
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "              ucoro C++23 wrapper benchmarks                \n";
    std::cout << "═══════════════════════════════════════════════════════════════\n\n";

    bench_memory_overhead();
    bench_allocation_pattern();
    bench_create_destroy();
    bench_context_switch();
    bench_storage_push_pop();
    bench_generator_iteration();

    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "                     benchmarks complete                       \n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    return 0;
}
