// basic.cpp - basic coroutine usage example
// the "hello world" of coroutines, if hello world involved context switching

#define UCORO_IMPL
#include "ucoro/ucoro.hpp"

#include <cstdio> // for stderr
#include <print>

int main()
{
    std::println("=== basic coroutine example ===\n");

    // create a simple coroutine that yields a few times
    auto coro_result = coro::coroutine::create([](coro::coroutine_handle h)
                                               {
        std::println("coroutine: starting");
        
        std::println("coroutine: doing some work...");
        [[maybe_unused]] auto _ = h.yield();
        
        std::println("coroutine: resumed, doing more work...");
        [[maybe_unused]] auto __ = h.yield();
        
        std::println("coroutine: finishing up"); });

    if (!coro_result)
    {
        std::println(stderr, "failed to create coroutine: {}", coro::to_string(coro_result.error()));
        return 1;
    }

    auto &coro = *coro_result;

    std::println("main: coroutine created, status = {}", coro::to_string(coro.status()));

    // resume until completion
    int step = 1;
    while (!coro.done())
    {
        std::println("\nmain: resuming coroutine (step {})", step++);

        auto resume_result = coro.resume();
        if (!resume_result)
        {
            std::println(stderr, "resume failed: {}", coro::to_string(resume_result.error()));
            return 1;
        }

        std::println("main: coroutine yielded, status = {}", coro::to_string(coro.status()));
    }

    std::println("\nmain: coroutine completed");

    // demonstrate storage (data passing)
    std::println("\n=== data passing example ===\n");

    auto data_coro = coro::coroutine::create([](coro::coroutine_handle h)
                                             {
        // receive data from main
        auto value = h.pop<int>();
        if (value) {
            std::println("coroutine: received value = {}", *value);
            
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
            std::println("main: received result = {}", *result);
        }
    }

    std::println("\ndone!");
    return 0;
}