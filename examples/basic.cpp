// basic.cpp - basic coroutine usage example
// the "hello world" of coroutines, if hello world involved context switching

#define UCORO_IMPL
#include "ucoro/impl/minicoro_impl.h"
#include "ucoro/ucoro.hpp"

#include <iostream>

int main()
{
    std::cout << "=== basic coroutine example ===\n\n";

    // create a simple coroutine that yields a few times
    auto coro_result = coro::coroutine::create([](coro::coroutine_handle h)
                                               {
        std::cout << "coroutine: starting\n";
        
        std::cout << "coroutine: doing some work...\n";
        [[maybe_unused]] auto _ = h.yield();
        
        std::cout << "coroutine: resumed, doing more work...\n";
        [[maybe_unused]] auto __ = h.yield();
        
        std::cout << "coroutine: finishing up\n"; });

    if (!coro_result)
    {
        std::cerr << "failed to create coroutine: " << coro::to_string(coro_result.error()) << "\n";
        return 1;
    }

    auto &coro = *coro_result;

    std::cout << "main: coroutine created, status = " << coro::to_string(coro.status()) << "\n";

    // resume until completion
    int step = 1;
    while (!coro.done())
    {
        std::cout << "\nmain: resuming coroutine (step " << step++ << ")\n";

        auto resume_result = coro.resume();
        if (!resume_result)
        {
            std::cerr << "resume failed: " << coro::to_string(resume_result.error()) << "\n";
            return 1;
        }

        std::cout << "main: coroutine yielded, status = " << coro::to_string(coro.status()) << "\n";
    }

    std::cout << "\nmain: coroutine completed\n";

    // demonstrate storage (data passing)
    std::cout << "\n=== data passing example ===\n\n";

    auto data_coro = coro::coroutine::create([](coro::coroutine_handle h)
                                             {
        // receive data from main
        auto value = h.pop<int>();
        if (value) {
            std::cout << "coroutine: received value = " << *value << "\n";
            
            // modify and send back
            [[maybe_unused]] auto _ = h.push(*value * 2);
        } });

    if (data_coro)
    {
        // send data to coroutine
        data_coro->push(21);

        // run coroutine
        data_coro->resume();

        // receive result
        auto result = data_coro->pop<int>();
        if (result)
        {
            std::cout << "main: received result = " << *result << "\n";
        }
    }

    std::cout << "\ndone!\n";
    return 0;
}
