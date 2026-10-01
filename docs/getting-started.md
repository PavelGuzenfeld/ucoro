# Getting started

## Installation

**Option 1: CMake FetchContent**
```cmake
include(FetchContent)
FetchContent_Declare(ucoro
    GIT_REPOSITORY https://github.com/PavelGuzenfeld/ucoro.git
    GIT_TAG v0.1.1
)
FetchContent_MakeAvailable(ucoro)

target_link_libraries(your_target PRIVATE ucoro::ucoro)
```

**Option 2: Copy the header**
```bash
# Copy include/ucoro/ucoro.hpp to your project — no other files needed
```

## First coroutine

```cpp
// main.cpp
#define UCORO_IMPL  // in exactly one source file
#include <ucoro/ucoro.hpp>
#include <cstdio>

int main() {
    auto result = coro::coroutine::create([](coro::coroutine_handle h) {
        std::puts("step 1");
        (void)h.yield();
        std::puts("step 2");
        (void)h.yield();
        std::puts("step 3");
    });

    if (!result) {
        std::fprintf(stderr, "error: %.*s\n",
            static_cast<int>(coro::to_string(result.error()).size()),
            coro::to_string(result.error()).data());
        return 1;
    }

    auto& coro = *result;
    while (!coro.done()) {
        (void)coro.resume();
    }
    // Output: step 1, step 2, step 3
}
```

## Building

### Requirements

- C++23 compiler (GCC 13+, Clang 18+, MSVC 2022+)
- CMake 3.22+
- [fmt](https://github.com/fmtlib/fmt) (only for tests/benchmarks/examples; fetched automatically)

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### CMake options

| Option                    | Default | Description                |
| ------------------------- | ------- | -------------------------- |
| `UCORO_BUILD_TESTS`       | `ON`    | Build test suite           |
| `UCORO_BUILD_BENCHMARKS`  | `ON`    | Build benchmarks           |
| `UCORO_BUILD_EXAMPLES`    | `ON`    | Build examples             |
| `UCORO_ENABLE_SANITIZERS` | `ON`    | Enable ASan/UBSan in Debug |

## Platforms

| Platform | Architecture          | Compiler           | Status          |
| -------- | --------------------- | ------------------ | --------------- |
| Linux    | x86_64                | GCC 13+, Clang 18+ | Tested in CI    |
| Linux    | ARM64                 | GCC 13+, Clang 18+ | Supported       |
| macOS    | ARM64 (Apple Silicon) | Apple Clang 15+    | Tested in CI    |
| macOS    | x86_64                | Apple Clang 15+    | Supported       |
| Windows  | x64                   | MSVC 2022+         | Tested in CI    |
