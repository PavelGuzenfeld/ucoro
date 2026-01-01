// generator.cpp - python-style generator example
// because yield is the new return

#define UCORO_IMPL
#include "ucoro/impl/minicoro_impl.h"
#include "ucoro/ucoro.hpp"

#include <cmath>
#include <iostream>

int main()
{
    std::cout << "=== generator example ===\n\n";

    // fibonacci generator
    std::cout << "fibonacci sequence:\n  ";
    {
        auto fib_gen = coro::generator<int>::create([](coro::coroutine_handle h)
                                                    {
            int a = 0, b = 1;
            for (int i = 0; i < 15; ++i) {
                [[maybe_unused]] auto _ = coro::yield_value(h, a);
                int next = a + b;
                a = b;
                b = next;
            } });

        if (fib_gen)
        {
            for (auto const &value : *fib_gen)
            {
                std::cout << value << " ";
            }
            std::cout << "\n";
        }
    }

    // prime number generator
    std::cout << "\nprime numbers up to 50:\n  ";
    {
        auto is_prime = [](int n) -> bool
        {
            if (n < 2)
                return false;
            if (n == 2)
                return true;
            if (n % 2 == 0)
                return false;
            for (int i = 3; i <= static_cast<int>(std::sqrt(n)); i += 2)
            {
                if (n % i == 0)
                    return false;
            }
            return true;
        };

        auto prime_gen = coro::generator<int>::create([&is_prime](coro::coroutine_handle h)
                                                      {
            for (int n = 2; n <= 50; ++n) {
                if (is_prime(n)) {
                    [[maybe_unused]] auto _ = coro::yield_value(h, n);
                }
            } });

        if (prime_gen)
        {
            for (auto const &prime : *prime_gen)
            {
                std::cout << prime << " ";
            }
            std::cout << "\n";
        }
    }

    // squares generator with sum
    std::cout << "\nsquares of 1-10:\n  ";
    {
        auto squares = coro::generator<int>::create([](coro::coroutine_handle h)
                                                    {
            for (int i = 1; i <= 10; ++i) {
                [[maybe_unused]] auto _ = coro::yield_value(h, i * i);
            } });

        if (squares)
        {
            int sum = 0;
            for (auto const &sq : *squares)
            {
                std::cout << sq << " ";
                sum += sq;
            }
            std::cout << "\n  sum = " << sum << "\n";
        }
    }

    std::cout << "\ndone!\n";
    return 0;
}
