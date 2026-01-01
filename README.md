# µcoro (ucoro)

A modern C++23 coroutine library providing **stackful coroutines** with blazing-fast context switching. Header-only, zero dependencies (except fmt), cross-platform.

[![CI](https://github.com/user/ucoro/actions/workflows/ci.yml/badge.svg)](https://github.com/user/ucoro/actions/workflows/ci.yml)
[![Sanitizers](https://github.com/user/ucoro/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/user/ucoro/actions/workflows/sanitizers.yml)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-MIT%2FUnlicense-green.svg)](LICENSE)
[![Header Only](https://img.shields.io/badge/header--only-yes-brightgreen.svg)]()
[![Platform](https://img.shields.io/badge/platform-windows%20%7C%20linux%20%7C%20macos-lightgrey.svg)]()

| Compiler | Minimum Version |
|----------|-----------------|
| GCC | 13+ |
| Clang | 18+ |
| MSVC | 2022 (19.38+) |
| Apple Clang | 15+ |

## Features

- **~55ns context switches** - up to 50x faster than POSIX `ucontext`, faster than Boost.Context
- **Modern C++23 API** - `std::expected`, concepts, strong types, `[[nodiscard]]`
- **Header-only** - single header, define `UCORO_IMPL` in one TU
- **Zero-overhead abstractions** - safe API adds minimal overhead vs raw C
- **Cross-platform** - Windows x64, Linux x64/ARM64, macOS x64/ARM64
- **Generators** - Python-style generators with range-for support
- **Task runner** - cooperative round-robin scheduler
- **Type-safe storage** - LIFO data passing between coroutine and caller

## Quick Start

### Installation

**Option 1: CMake FetchContent**
```cmake
include(FetchContent)
FetchContent_Declare(ucoro
    GIT_REPOSITORY https://github.com/user/ucoro.git
    GIT_TAG main
)
FetchContent_MakeAvailable(ucoro)

target_link_libraries(your_target PRIVATE ucoro::ucoro)
```

**Option 2: Copy the header**
```bash
# Copy include/ucoro/ucoro.hpp to your project
```

### Basic Usage

```cpp
// main.cpp
#define UCORO_IMPL  // Define in exactly ONE source file
#include <ucoro/ucoro.hpp>
#include <fmt/core.h>

int main() {
    // Create a coroutine
    auto result = coro::coroutine::create([](coro::coroutine_handle h) {
        fmt::println("step 1");
        (void)h.yield();
        fmt::println("step 2");
        (void)h.yield();
        fmt::println("step 3");
    });

    if (!result) {
        fmt::println(stderr, "error: {}", result.error());
        return 1;
    }

    auto& coro = *result;

    while (!coro.done()) {
        (void)coro.resume();
    }
    // Output: step 1, step 2, step 3
}
```

### Generators

```cpp
auto fib = coro::generator<int>::create([](coro::coroutine_handle h) {
    int a = 0, b = 1;
    while (true) {
        (void)coro::yield_value(h, a);
        int next = a + b;
        a = b;
        b = next;
    }
});

// Range-for support
for (int value : *fib) {
    fmt::println("{}", value);
    if (value > 100) break;
}
// Output: 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144
```

### Data Passing (Storage)

```cpp
auto coro = coro::coroutine::create([](coro::coroutine_handle h) {
    // Pop data pushed by caller (LIFO order)
    auto value = h.pop<int>();
    if (value) {
        fmt::println("received: {}", *value);
    }
});

// Push data before resuming
(void)coro->push(42);
(void)coro->resume();
// Output: received: 42
```

### Task Runner (Cooperative Multitasking)

```cpp
coro::task_runner runner;

runner.add(std::move(*coro::coroutine::create([](coro::coroutine_handle h) {
    fmt::println("task A: step 1");
    (void)h.yield();
    fmt::println("task A: step 2");
})));

runner.add(std::move(*coro::coroutine::create([](coro::coroutine_handle h) {
    fmt::println("task B: step 1");
    (void)h.yield();
    fmt::println("task B: step 2");
})));

(void)runner.run();
// Output (interleaved): task A: step 1, task B: step 1, task A: step 2, task B: step 2
```

### Unchecked API (Maximum Performance)

For hot paths where you've already validated state:

```cpp
auto coro = coro::coroutine::create([](coro::coroutine_handle h) {
    while (true) {
        int val = h.pop_unchecked<int>();
        h.push_unchecked(val * 2);
        h.yield_unchecked();
    }
});

// Fast path - no error checking
coro->push_unchecked(21);
coro->resume_unchecked();
int result = coro->pop_unchecked<int>(); // 42
```

## API Reference

### Error Handling

All fallible operations return `std::expected<T, coro::error>`:

```cpp
enum class error : std::uint8_t {
    success,
    generic_error,
    invalid_pointer,
    invalid_coroutine,
    not_suspended,
    not_running,
    make_context_error,
    switch_context_error,
    not_enough_space,
    out_of_memory,
    invalid_arguments,
    invalid_operation,
    stack_overflow
};
```

### Coroutine States

```cpp
enum class state : std::uint8_t {
    dead,       // Completed or never started
    normal,     // Resumed another coroutine
    running,    // Currently executing
    suspended   // Yielded, waiting to resume
};
```

### Configuration

```cpp
// Compile-time configuration (define before including ucoro.hpp)
#define UCORO_STACK_SIZE    (56 * 1024)  // Default stack size
#define UCORO_MIN_STACK_SIZE 32768       // Minimum allowed
#define UCORO_STORAGE_SIZE   1024        // Default storage size

// Runtime configuration via strong types
auto coro = coro::coroutine::create(
    my_func,
    coro::stack_size{128 * 1024},    // Custom stack
    coro::storage_size{4096}         // Custom storage
);
```

### Concepts

```cpp
// Type must be trivially copyable, standard layout, and <= 1024 bytes
template <typename T>
concept storable = std::is_trivially_copyable_v<T> 
                && std::is_standard_layout_v<T> 
                && (sizeof(T) <= 1024);
```

## Benchmarks

Context switch performance compared to POSIX `ucontext` and Boost.Context.

### Context Switch Latency (median, lower is better)

| Platform | ucoro Safe | ucoro Unchecked | Boost.Context | ucontext | Speedup vs ucontext |
|----------|------------|-----------------|---------------|----------|---------------------|
| **Linux x64** (Local, GCC 13) | 91 ns | **55 ns** | 72 ns | 352 ns | **6.4x** |
| **Windows x64** (CI, MSVC) | 100 ns | 100 ns | N/A | N/A | - |
| **macOS ARM64** (CI, Apple Clang) | 42 ns | 42 ns | N/A | 1,625 ns | **~39x** |
| **Ubuntu x64** (CI, Clang 18) | 40 ns | 40 ns | N/A | 652 ns | **~16x** |
| **Ubuntu x64** (CI, GCC 13) | 50 ns | 40 ns | N/A | 661 ns | **~17x** |

### Context Switch Throughput (ops/sec, higher is better)

| Platform | ucoro Safe | ucoro Unchecked | Boost.Context | ucontext |
|----------|------------|-----------------|---------------|----------|
| **Linux x64** (Local) | 10.5M | **17.5M** | 13.1M | 2.7M |
| **Windows x64** (CI) | 18.1M | 18.2M | N/A | N/A |
| **macOS ARM64** (CI) | 30.3M | 30.8M | N/A | 611K |
| **Ubuntu x64** (CI, Clang 18) | 23.2M | 22.1M | N/A | 1.51M |
| **Ubuntu x64** (CI, GCC 13) | 20.8M | 23.4M | N/A | 1.47M |

### Performance Comparison Chart

```
Context Switch Latency (lower = better)
═══════════════════════════════════════════════════════════════

ucoro Unchecked │████████████                          55 ns
Boost.Context   │█████████████████                     72 ns  
ucoro Safe      │█████████████████████                 91 ns
ucontext POSIX  │████████████████████████████████████ 352 ns

═══════════════════════════════════════════════════════════════
                 ucoro unchecked is 1.3x faster than Boost
                 ucoro unchecked is 6.4x faster than ucontext
```

### Create + Destroy Throughput (ops/sec)

| Platform | ucoro C++ | ucoro Raw C |
|----------|-----------|-------------|
| **Linux x64** (Local) | 1.06M | 1.15M |
| **Windows x64** (CI) | 797K | 898K |
| **macOS ARM64** (CI) | 1.40M | 1.39M |
| **Ubuntu x64** (CI, Clang 18) | 1.37M | 1.42M |
| **Ubuntu x64** (CI, GCC 13) | 1.06M | 1.32M |

### Storage Operations (push + pop + context switch)

| Platform | ucoro Safe | ucoro Unchecked | Raw C API |
|----------|------------|-----------------|-----------|
| **Linux x64** (Local) | 8.3M ops/s | **14.9M ops/s** | 13.0M ops/s |

### Memory Overhead

| Type | Size |
|------|------|
| `coro::coroutine` | 16 bytes |
| `coro::coroutine_handle` | 8 bytes |
| `coro::task_runner` | 24 bytes |
| Internal `mco_coro` | 136 bytes |
| Default stack | 56 KB |
| Default storage | 1 KB |

## Building

### Requirements

- C++23 compiler (GCC 13+, Clang 18+, MSVC 2022+)
- CMake 3.22+
- [fmt](https://github.com/fmtlib/fmt) library (fetched automatically)

### Build Commands

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Test
ctest --test-dir build -C Release --output-on-failure

# Run benchmarks
./build/benchmark_ucoro      # Unix
.\build\Release\benchmark_ucoro.exe  # Windows
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `UCORO_BUILD_TESTS` | `ON` | Build test suite |
| `UCORO_BUILD_BENCHMARKS` | `ON` | Build benchmarks |
| `UCORO_BUILD_EXAMPLES` | `ON` | Build examples |
| `UCORO_ENABLE_SANITIZERS` | `ON` | Enable ASan/UBSan in Debug |

## Platform Support

| Platform | Architecture | Compiler | Status |
|----------|--------------|----------|--------|
| Linux | x86_64 | GCC 13+, Clang 18+ | ✅ Tested in CI |
| Linux | ARM64 | GCC 13+, Clang 18+ | ✅ Supported |
| macOS | x86_64 | Apple Clang 15+ | ✅ Supported |
| macOS | ARM64 (Apple Silicon) | Apple Clang 15+ | ✅ Tested in CI |
| Windows | x64 | MSVC 2022+ | ✅ Tested in CI |
| Windows | ARM64 | MSVC | ❌ Not yet |

## How It Works

ucoro uses hand-written assembly for context switching on each platform:

- **x86_64**: Saves/restores RBP, RBX, R12-R15, RSP, RIP (8 registers)
- **ARM64**: Saves/restores X19-X30, SP, LR, D8-D15 (callee-saved)
- **Windows x64**: Additionally saves XMM6-XMM15 and TEB fields

The assembly is embedded directly in the header via `__asm__` blocks (Unix) or raw byte arrays (Windows), requiring no external assembler.

## Thread Safety

- Each coroutine should only be accessed from one thread at a time
- `coro::running()` is thread-local - returns the coroutine executing on the current thread
- `task_runner` is not thread-safe - use one per thread or add external synchronization
- Creating coroutines from multiple threads is safe (uses standard allocators)

## Why Not Use C++20 Coroutines?

C++20 coroutines are **stackless** - they can only suspend at explicit `co_await`/`co_yield` points. ucoro provides **stackful** coroutines that can suspend from any call depth, which is essential for:

- Wrapping legacy callback-based APIs
- Implementing green threads / fibers
- Game loop architectures
- Cooperative multitasking without viral `async`/`await`

## License

MIT OR Unlicense (your choice)

Based on [minicoro](https://github.com/edubart/minicoro) by Eduardo Bart.

## Acknowledgments

- [Eduardo Bart](https://github.com/edubart) - Original minicoro C library
- [Boost.Context](https://www.boost.org/doc/libs/release/libs/context/) - Inspiration for assembly routines