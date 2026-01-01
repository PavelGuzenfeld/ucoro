// basic.cpp - basic coroutine usage example
// the "hello world" of coroutines, if hello world involved context switching

#define UCORO_IMPL
#include "ucoro/ucoro.hpp"

#include <cstdio> // for stderr
#include <fmt/core.h>

int main()
{
    fmt::print("=== basic coroutine example ===\n");

    // create a simple coroutine that yields a few times
    auto coro_result = coro::coroutine::create([](coro::coroutine_handle h)
                                               {
        fmt::print("coroutine: starting");
        
        fmt::print("coroutine: doing some work...");
        [[maybe_unused]] auto _ = h.yield();
        
        fmt::print("coroutine: resumed, doing more work...");
        [[maybe_unused]] auto __ = h.yield();
        
        fmt::print("coroutine: finishing up"); });

    if (!coro_result)
    {
        fmt::print(stderr, "failed to create coroutine: {}", coro::to_string(coro_result.error()));
        return 1;
    }

    auto &coro = *coro_result;

    fmt::print("main: coroutine created, status = {}", coro::to_string(coro.status()));

    // resume until completion
    int step = 1;
    while (!coro.done())
    {
        fmt::print("\nmain: resuming coroutine (step {})", step++);

        auto resume_result = coro.resume();
        if (!resume_result)
        {
            fmt::print(stderr, "resume failed: {}", coro::to_string(resume_result.error()));
            return 1;
        }

        fmt::print("main: coroutine yielded, status = {}", coro::to_string(coro.status()));
    }

    fmt::print("\nmain: coroutine completed");

    // demonstrate storage (data passing)
    fmt::print("\n=== data passing example ===\n");

    auto data_coro = coro::coroutine::create([](coro::coroutine_handle h)
                                             {
        // receive data from main
        auto value = h.pop<int>();
        if (value) {
            fmt::print("coroutine: received value = {}", *value);
            
            // modify and send back
            [[maybe_unused]] auto _ = h.push(*value * 2);
        } });

    if (data_coro)
    {
        // send data to coroutine
        (void)data_coro->push(21);

        // run coroutine
        (void)data_coro->resume();

        // receive result
        auto result = data_coro->pop<int>();
        if (result)
        {
            fmt::print("main: received result = {}", *result);
        }
    }

    fmt::print("\ndone!");
    return 0;
}