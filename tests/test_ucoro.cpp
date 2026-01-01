// test_ucoro.cpp - tests for the C++23 ucoro wrapper
// using doctest because gtest is bloated and catch2 compiles like a drunk turtle
//
// compile with: g++ -std=c++23 -I../include -I../src -o test_ucoro test_ucoro.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// define implementation before including the wrapper
#define UCORO_IMPL
#include "ucoro/impl/minicoro_impl.h"
#include "ucoro/ucoro.hpp"

#include <numeric>
#include <string>
#include <vector>

// ============================================================================
// error enum tests - because even enums deserve validation
// ============================================================================

TEST_SUITE("error enum")
{
    TEST_CASE("to_string returns correct values")
    {
        CHECK(coro::to_string(coro::error::success) == "success");
        CHECK(coro::to_string(coro::error::generic_error) == "generic error");
        CHECK(coro::to_string(coro::error::invalid_pointer) == "invalid pointer");
        CHECK(coro::to_string(coro::error::invalid_coroutine) == "invalid coroutine");
        CHECK(coro::to_string(coro::error::not_suspended) == "coroutine not suspended");
        CHECK(coro::to_string(coro::error::not_running) == "coroutine not running");
        CHECK(coro::to_string(coro::error::make_context_error) == "make context error");
        CHECK(coro::to_string(coro::error::switch_context_error) == "switch context error");
        CHECK(coro::to_string(coro::error::not_enough_space) == "not enough space");
        CHECK(coro::to_string(coro::error::out_of_memory) == "out of memory");
        CHECK(coro::to_string(coro::error::invalid_arguments) == "invalid arguments");
        CHECK(coro::to_string(coro::error::invalid_operation) == "invalid operation");
        CHECK(coro::to_string(coro::error::stack_overflow) == "stack overflow");
    }

    TEST_CASE("comparison operators work")
    {
        CHECK(coro::error::success == coro::error::success);
        CHECK(coro::error::success != coro::error::generic_error);
    }
}

// ============================================================================
// state enum tests
// ============================================================================

TEST_SUITE("state enum")
{
    TEST_CASE("to_string returns correct values")
    {
        CHECK(coro::to_string(coro::state::dead) == "dead");
        CHECK(coro::to_string(coro::state::normal) == "normal");
        CHECK(coro::to_string(coro::state::running) == "running");
        CHECK(coro::to_string(coro::state::suspended) == "suspended");
    }

    TEST_CASE("comparison operators work")
    {
        CHECK(coro::state::dead == coro::state::dead);
        CHECK(coro::state::running != coro::state::suspended);
    }
}

// ============================================================================
// strong type tests - because raw size_t is for savages
// ============================================================================

TEST_SUITE("strong types")
{
    TEST_CASE("stack_size holds correct value")
    {
        constexpr coro::stack_size size{1024};
        CHECK(size.value == 1024);
    }

    TEST_CASE("storage_size holds correct value")
    {
        constexpr coro::storage_size size{512};
        CHECK(size.value == 512);
    }

    TEST_CASE("default values are sensible")
    {
        CHECK(coro::default_stack_size.value == 56UL * 1024UL);
        CHECK(coro::default_storage_size.value == 1024UL);
        CHECK(coro::min_stack_size.value == 32768UL);
    }
}

// ============================================================================
// coroutine_handle tests - the non-owning view
// ============================================================================

TEST_SUITE("coroutine_handle")
{
    TEST_CASE("default constructed handle is invalid")
    {
        coro::coroutine_handle h;
        CHECK_FALSE(h.valid());
        CHECK_FALSE(static_cast<bool>(h));
        CHECK(h.raw() == nullptr);
    }

    TEST_CASE("yield on invalid handle returns error")
    {
        coro::coroutine_handle h;
        auto result = h.yield();
        CHECK_FALSE(result.has_value());
        CHECK(result.error() == coro::error::invalid_coroutine);
    }
}

// ============================================================================
// coroutine tests - the main event
// ============================================================================

TEST_SUITE("coroutine")
{
    TEST_CASE("create with null function returns error")
    {
        auto result = coro::coroutine::create(nullptr);
        CHECK_FALSE(result.has_value());
        CHECK(result.error() == coro::error::invalid_arguments);
    }

    TEST_CASE("create simple coroutine succeeds")
    {
        bool executed = false;
        auto result = coro::coroutine::create([&executed](coro::coroutine_handle)
                                              { executed = true; });

        CHECK(result.has_value());
        auto &coro = *result;

        CHECK(coro.valid());
        CHECK(coro.suspended());
        CHECK_FALSE(coro.done());
        CHECK_FALSE(executed);
    }

    TEST_CASE("resume executes coroutine")
    {
        bool executed = false;
        auto result = coro::coroutine::create([&executed](coro::coroutine_handle)
                                              { executed = true; });

        REQUIRE(result.has_value());
        auto &coro = *result;

        auto resume_result = coro.resume();
        CHECK(resume_result.has_value());
        CHECK(executed);
        CHECK(coro.done());
    }

    TEST_CASE("yield suspends coroutine")
    {
        int step = 0;
        auto result = coro::coroutine::create([&step](coro::coroutine_handle h)
                                              {
            step = 1;
            [[maybe_unused]] auto _ = h.yield();
            step = 2;
            [[maybe_unused]] auto __ = h.yield();
            step = 3; });

        REQUIRE(result.has_value());
        auto &coro = *result;

        CHECK(step == 0);

        (void)coro.resume();
        CHECK(step == 1);
        CHECK(coro.suspended());

        (void)coro.resume();
        CHECK(step == 2);
        CHECK(coro.suspended());

        (void)coro.resume();
        CHECK(step == 3);
        CHECK(coro.done());
    }

    TEST_CASE("multiple yields work correctly")
    {
        std::vector<int> values;
        auto result = coro::coroutine::create([&values](coro::coroutine_handle h)
                                              {
            for (int i = 0; i < 5; ++i) {
                values.push_back(i);
                if (i < 4) {
                    [[maybe_unused]] auto _ = h.yield();
                }
            } });

        REQUIRE(result.has_value());
        auto &coro = *result;

        for (int i = 0; i < 5; ++i)
        {
            (void)coro.resume();
            CHECK(values.size() == static_cast<size_t>(i + 1));
        }

        CHECK(coro.done());
        CHECK((values == std::vector<int>{0, 1, 2, 3, 4}));
    }

    TEST_CASE("move constructor transfers ownership")
    {
        auto result = coro::coroutine::create([](coro::coroutine_handle) {});
        REQUIRE(result.has_value());

        auto coro1 = std::move(*result);
        CHECK(coro1.valid());

        auto coro2 = std::move(coro1);
        CHECK(coro2.valid());
        CHECK_FALSE(coro1.valid()); // moved-from state
    }

    TEST_CASE("move assignment transfers ownership")
    {
        auto result1 = coro::coroutine::create([](coro::coroutine_handle) {});
        auto result2 = coro::coroutine::create([](coro::coroutine_handle) {});
        REQUIRE(result1.has_value());
        REQUIRE(result2.has_value());

        auto coro1 = std::move(*result1);
        auto coro2 = std::move(*result2);

        coro1 = std::move(coro2);
        CHECK(coro1.valid());
        CHECK_FALSE(coro2.valid());
    }

    TEST_CASE("resume on invalid coroutine returns error")
    {
        auto result = coro::coroutine::create([](coro::coroutine_handle) {});
        REQUIRE(result.has_value());

        auto coro1 = std::move(*result);
        auto coro2 = std::move(coro1); // coro1 is now invalid

        auto resume_result = coro1.resume();
        CHECK_FALSE(resume_result.has_value());
        CHECK(resume_result.error() == coro::error::invalid_coroutine);
    }

    TEST_CASE("custom stack size works")
    {
        auto result = coro::coroutine::create(
            [](coro::coroutine_handle) {},
            coro::stack_size{64 * 1024});
        CHECK(result.has_value());
    }

    TEST_CASE("custom storage size works")
    {
        auto result = coro::coroutine::create(
            [](coro::coroutine_handle) {},
            coro::default_stack_size,
            coro::storage_size{2048});
        CHECK(result.has_value());
        CHECK(result->storage_capacity() == 2048);
    }
}

// ============================================================================
// storage tests - push/pop/peek operations
// ============================================================================

TEST_SUITE("storage")
{
    TEST_CASE("push and pop int")
    {
        int received = 0;
        auto result = coro::coroutine::create([&received](coro::coroutine_handle h)
                                              {
            auto pop_result = h.pop<int>();
            if (pop_result) {
                received = *pop_result;
            } });

        REQUIRE(result.has_value());
        auto &coro = *result;

        auto push_result = coro.push(42);
        CHECK(push_result.has_value());

        (void)coro.resume();
        CHECK(received == 42);
    }

    TEST_CASE("push and pop struct")
    {
        struct TestData
        {
            int a;
            float b;
            char c;
        };

        TestData received{};
        auto result = coro::coroutine::create([&received](coro::coroutine_handle h)
                                              {
            auto pop_result = h.pop<TestData>();
            if (pop_result) {
                received = *pop_result;
            } });

        REQUIRE(result.has_value());
        auto &coro = *result;

        TestData sent{.a = 123, .b = 3.14f, .c = 'X'};
        auto push_result = coro.push(sent);
        CHECK(push_result.has_value());

        (void)coro.resume();
        CHECK(received.a == 123);
        CHECK(received.b == doctest::Approx(3.14f));
        CHECK(received.c == 'X');
    }

    TEST_CASE("peek does not consume data")
    {
        auto result = coro::coroutine::create([](coro::coroutine_handle h)
                                              {
            // peek twice, pop once
            auto peek1 = h.peek<int>();
            auto peek2 = h.peek<int>();
            auto pop1 = h.pop<int>();
            
            CHECK(peek1.has_value());
            CHECK(peek2.has_value());
            CHECK(pop1.has_value());
            
            CHECK(*peek1 == 42);
            CHECK(*peek2 == 42);
            CHECK(*pop1 == 42); });

        REQUIRE(result.has_value());
        auto &coro = *result;

        (void)coro.push(42);
        (void)coro.resume();
    }

    TEST_CASE("bytes_stored tracks usage")
    {
        auto result = coro::coroutine::create([](coro::coroutine_handle h)
                                              { [[maybe_unused]] auto _ = h.yield(); });

        REQUIRE(result.has_value());
        auto &coro = *result;

        CHECK(coro.bytes_stored() == 0);

        (void)coro.push(42);
        CHECK(coro.bytes_stored() == sizeof(int));

        (void)coro.push(3.14);
        CHECK(coro.bytes_stored() == sizeof(int) + sizeof(double));
    }

    TEST_CASE("pop on empty storage returns error")
    {
        auto result = coro::coroutine::create([](coro::coroutine_handle h)
                                              {
            auto pop_result = h.pop<int>();
            CHECK_FALSE(pop_result.has_value());
            CHECK(pop_result.error() == coro::error::not_enough_space); });

        REQUIRE(result.has_value());
        (void)result->resume();
    }

    TEST_CASE("multiple values LIFO order")
    {
        // storage is LIFO (stack)
        auto result = coro::coroutine::create([](coro::coroutine_handle h)
                                              {
            auto v3 = h.pop<int>();
            auto v2 = h.pop<int>();
            auto v1 = h.pop<int>();
            
            CHECK(v1.has_value());
            CHECK(v2.has_value());
            CHECK(v3.has_value());
            
            CHECK(*v1 == 1);
            CHECK(*v2 == 2);
            CHECK(*v3 == 3); });

        REQUIRE(result.has_value());
        auto &coro = *result;

        // push in order 1, 2, 3
        (void)coro.push(1);
        (void)coro.push(2);
        (void)coro.push(3);

        // pop will be 3, 2, 1 (LIFO)
        (void)coro.resume();
    }
}

// ============================================================================
// generator tests - python-style yield
// ============================================================================

TEST_SUITE("generator")
{
    TEST_CASE("generate sequence of ints")
    {
        auto gen_result = coro::generator<int>::create([](coro::coroutine_handle h)
                                                       {
            for (int i = 0; i < 5; ++i) {
                [[maybe_unused]] auto _ = coro::yield_value(h, i);
            } });

        REQUIRE(gen_result.has_value());
        auto &gen = *gen_result;

        std::vector<int> collected;
        while (auto next = gen.next())
        {
            if (*next)
            {
                collected.push_back(**next);
            }
            else
            {
                break;
            }
        }

        CHECK((collected == std::vector<int>{0, 1, 2, 3, 4}));
    }

    TEST_CASE("generator with range-for")
    {
        auto gen_result = coro::generator<int>::create([](coro::coroutine_handle h)
                                                       {
            for (int i = 10; i < 15; ++i) {
                [[maybe_unused]] auto _ = coro::yield_value(h, i);
            } });

        REQUIRE(gen_result.has_value());
        auto &gen = *gen_result;

        std::vector<int> collected;
        for (auto const &value : gen)
        {
            collected.push_back(value);
        }

        CHECK((collected == std::vector<int>{10, 11, 12, 13, 14}));
    }

    TEST_CASE("empty generator")
    {
        auto gen_result = coro::generator<int>::create([](coro::coroutine_handle)
                                                       {
                                                           // yield nothing
                                                       });

        REQUIRE(gen_result.has_value());
        auto &gen = *gen_result;

        auto next = gen.next();
        CHECK(next.has_value());
        CHECK_FALSE(next->has_value()); // optional is empty
        CHECK(gen.done());
    }

    TEST_CASE("fibonacci generator")
    {
        auto gen_result = coro::generator<int>::create([](coro::coroutine_handle h)
                                                       {
            int a = 0, b = 1;
            for (int i = 0; i < 10; ++i) {
                [[maybe_unused]] auto _ = coro::yield_value(h, a);
                int next = a + b;
                a = b;
                b = next;
            } });

        REQUIRE(gen_result.has_value());

        std::vector<int> fibs;
        for (auto const &value : *gen_result)
        {
            fibs.push_back(value);
        }

        CHECK((fibs == std::vector<int>{0, 1, 1, 2, 3, 5, 8, 13, 21, 34}));
    }
}

// ============================================================================
// task_runner tests - cooperative multitasking
// ============================================================================

TEST_SUITE("task_runner")
{
    TEST_CASE("run single task")
    {
        bool executed = false;
        coro::task_runner runner;

        auto coro_result = coro::coroutine::create([&executed](coro::coroutine_handle)
                                                   { executed = true; });
        REQUIRE(coro_result.has_value());

        runner.add(std::move(*coro_result));
        CHECK(runner.size() == 1);

        auto run_result = runner.run();
        CHECK(run_result.has_value());
        CHECK(executed);
        CHECK(runner.empty());
    }

    TEST_CASE("run multiple tasks round-robin")
    {
        std::vector<int> execution_order;
        coro::task_runner runner;

        // task A yields twice
        auto coro_a = coro::coroutine::create([&execution_order](coro::coroutine_handle h)
                                              {
            execution_order.push_back(1);
            [[maybe_unused]] auto _ = h.yield();
            execution_order.push_back(3);
            [[maybe_unused]] auto __ = h.yield();
            execution_order.push_back(5); });

        // task B yields twice
        auto coro_b = coro::coroutine::create([&execution_order](coro::coroutine_handle h)
                                              {
            execution_order.push_back(2);
            [[maybe_unused]] auto _ = h.yield();
            execution_order.push_back(4);
            [[maybe_unused]] auto __ = h.yield();
            execution_order.push_back(6); });

        REQUIRE(coro_a.has_value());
        REQUIRE(coro_b.has_value());

        runner.add(std::move(*coro_a));
        runner.add(std::move(*coro_b));

        auto run_result = runner.run();
        CHECK(run_result.has_value());

        // round-robin should interleave execution
        CHECK((execution_order == std::vector<int>{1, 2, 3, 4, 5, 6}));
    }

    TEST_CASE("step runs one iteration")
    {
        int counter = 0;
        coro::task_runner runner;

        auto coro_result = coro::coroutine::create([&counter](coro::coroutine_handle h)
                                                   {
            ++counter;
            [[maybe_unused]] auto _ = h.yield();
            ++counter;
            [[maybe_unused]] auto __ = h.yield();
            ++counter; });
        REQUIRE(coro_result.has_value());

        runner.add(std::move(*coro_result));

        auto step1 = runner.step();
        CHECK(step1.has_value());
        CHECK(*step1 == true); // still alive
        CHECK(counter == 1);

        auto step2 = runner.step();
        CHECK(step2.has_value());
        CHECK(*step2 == true);
        CHECK(counter == 2);

        auto step3 = runner.step();
        CHECK(step3.has_value());
        CHECK(*step3 == false); // done
        CHECK(counter == 3);
    }

    TEST_CASE("empty runner")
    {
        coro::task_runner runner;
        CHECK(runner.empty());
        CHECK(runner.size() == 0);

        auto run_result = runner.run();
        CHECK(run_result.has_value()); // no error, just nothing to do
    }

    TEST_CASE("tasks finishing at different times")
    {
        std::vector<std::string> log;
        coro::task_runner runner;

        // short task
        auto short_task = coro::coroutine::create([&log](coro::coroutine_handle)
                                                  { log.push_back("short"); });

        // long task
        auto long_task = coro::coroutine::create([&log](coro::coroutine_handle h)
                                                 {
            log.push_back("long-1");
            [[maybe_unused]] auto _ = h.yield();
            log.push_back("long-2");
            [[maybe_unused]] auto __ = h.yield();
            log.push_back("long-3"); });

        REQUIRE(short_task.has_value());
        REQUIRE(long_task.has_value());

        runner.add(std::move(*short_task));
        runner.add(std::move(*long_task));

        (void)runner.run();

        // short task finishes first, long task continues
        CHECK(log.size() == 4);
        CHECK(log[0] == "short");
        CHECK(log[1] == "long-1");
        CHECK(log[2] == "long-2");
        CHECK(log[3] == "long-3");
    }
}

// ============================================================================
// format tests - std::format support
// ============================================================================

TEST_SUITE("formatting")
{
    TEST_CASE("format error enum")
    {
        auto str = std::format("{}", coro::error::success);
        CHECK(str == "success");

        str = std::format("{}", coro::error::out_of_memory);
        CHECK(str == "out of memory");
    }

    TEST_CASE("format state enum")
    {
        auto str = std::format("{}", coro::state::running);
        CHECK(str == "running");

        str = std::format("{}", coro::state::dead);
        CHECK(str == "dead");
    }

    TEST_CASE("format in context")
    {
        auto str = std::format("coroutine is {}", coro::state::suspended);
        CHECK(str == "coroutine is suspended");

        str = std::format("error: {}", coro::error::invalid_arguments);
        CHECK(str == "error: invalid arguments");
    }
}

// ============================================================================
// edge cases and stress tests
// ============================================================================

TEST_SUITE("edge cases")
{
    TEST_CASE("deeply nested yields")
    {
        int depth = 0;
        int const max_depth = 1000;

        auto result = coro::coroutine::create([&depth, max_depth](coro::coroutine_handle h)
                                              {
            for (int i = 0; i < max_depth; ++i) {
                ++depth;
                [[maybe_unused]] auto _ = h.yield();
            } });

        REQUIRE(result.has_value());
        auto &coro = *result;

        while (!coro.done())
        {
            (void)coro.resume();
        }

        CHECK(depth == max_depth);
    }

    TEST_CASE("coroutine that throws")
    {
        // the C library doesn't handle C++ exceptions well, but we should
        // at least not crash if the coroutine doesn't throw
        bool completed = false;
        auto result = coro::coroutine::create([&completed](coro::coroutine_handle)
                                              {
            // don't actually throw, just complete normally
            completed = true; });

        REQUIRE(result.has_value());
        (void)result->resume();
        CHECK(completed);
    }

    TEST_CASE("running() outside coroutine")
    {
        // when not inside a coroutine, running() returns invalid handle
        auto handle = coro::running();
        CHECK_FALSE(handle.valid());
    }

    TEST_CASE("running() inside coroutine")
    {
        bool valid_inside = false;
        auto result = coro::coroutine::create([&valid_inside](coro::coroutine_handle)
                                              {
            auto handle = coro::running();
            valid_inside = handle.valid(); });

        REQUIRE(result.has_value());
        (void)result->resume();
        CHECK(valid_inside);
    }

    TEST_CASE("large data transfer through storage")
    {
        struct BigData
        {
            std::array<int, 100> values;
        };

        BigData received{};
        auto result = coro::coroutine::create(
            [&received](coro::coroutine_handle h)
            {
                auto pop_result = h.pop<BigData>();
                if (pop_result)
                {
                    received = *pop_result;
                }
            },
            coro::default_stack_size,
            coro::storage_size{2048} // need more storage for BigData
        );

        REQUIRE(result.has_value());
        auto &coro = *result;

        BigData sent{};
        std::iota(sent.values.begin(), sent.values.end(), 0);

        auto push_result = coro.push(sent);
        CHECK(push_result.has_value());

        (void)coro.resume();

        for (std::size_t i = 0; i < 100; ++i)
        {
            CHECK(received.values[i] == static_cast<int>(i));
        }
    }
}

// ============================================================================
// concept constraint tests - compile-time only
// ============================================================================

TEST_SUITE("concepts")
{
    TEST_CASE("storable concept accepts trivial types")
    {
        static_assert(coro::storable<int>);
        static_assert(coro::storable<double>);
        static_assert(coro::storable<char>);

        struct TrivialStruct
        {
            int a;
            float b;
        };
        static_assert(coro::storable<TrivialStruct>);
    }

    TEST_CASE("storable concept rejects non-trivial types")
    {
        // std::string is not trivially copyable
        static_assert(!coro::storable<std::string>);

        // std::vector is not trivially copyable
        static_assert(!coro::storable<std::vector<int>>);
    }

    TEST_CASE("storable concept rejects oversized types")
    {
        struct TooBig
        {
            std::array<char, 2048> data; // > 1024 limit
        };
        static_assert(!coro::storable<TooBig>);
    }
}
