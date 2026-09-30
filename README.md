# ucoro

Stackful coroutines for C++23 in one header, built on [minicoro](https://github.com/edubart/minicoro). Yield from any call depth, not only from the coroutine body.

Status: maintenance only. Bug fixes and platform fixes are accepted.

[![CI](https://github.com/PavelGuzenfeld/ucoro/actions/workflows/ci.yml/badge.svg)](https://github.com/PavelGuzenfeld/ucoro/actions/workflows/ci.yml) [![Sanitizers](https://github.com/PavelGuzenfeld/ucoro/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/PavelGuzenfeld/ucoro/actions/workflows/sanitizers.yml) [![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23) [![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE) [![Header Only](https://img.shields.io/badge/header--only-yes-brightgreen.svg)]() [![Platform](https://img.shields.io/badge/platform-windows%20%7C%20linux%20%7C%20macos-lightgrey.svg)]()

**Documentation: https://pavelguzenfeld.com/ucoro/**

Requires GCC 13+, Clang 18+, MSVC 19.38+ or Apple Clang 15+.

## Install

```cmake
include(FetchContent)
FetchContent_Declare(ucoro
    GIT_REPOSITORY https://github.com/PavelGuzenfeld/ucoro.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(ucoro)
target_link_libraries(your_target PRIVATE ucoro::ucoro)
```

Or copy `include/ucoro/ucoro.hpp` into your tree. Define `UCORO_IMPL` in exactly one source file before including it.

## Example

```cpp
#define UCORO_IMPL
#include <ucoro/ucoro.hpp>
#include <cstdio>

int main() {
    auto fib = coro::generator<int>::create([](coro::coroutine_handle h) {
        int a = 0, b = 1;
        while (true) {
            (void)coro::yield_value(h, a);
            b = a + b;
            a = b - a;
        }
    });
    for (int value : *fib) {
        if (value > 100) break;
        std::printf("%d ", value);
    }
}
```

## More

- [Getting started](https://pavelguzenfeld.com/ucoro/getting-started/): install options, first coroutine, build and CMake options
- [Guide](https://pavelguzenfeld.com/ucoro/guide/): generators, storage, task runner, exceptions, unchecked API
- [Advanced examples](https://pavelguzenfeld.com/ucoro/examples/): yielding from deep calls, state machines, wrapping callbacks
- [API reference](https://pavelguzenfeld.com/ucoro/api/)
- [Safety](https://pavelguzenfeld.com/ucoro/safety/): exception capture, guard pages, overflow checks
- [Benchmarks](https://pavelguzenfeld.com/ucoro/benchmarks/)
- [Roadmap](ROADMAP.md)

## License

MIT. Based on minicoro by Eduardo Bart.
