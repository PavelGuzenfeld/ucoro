# ucoro

A modern C++23 coroutine library providing **stackful coroutines** with blazing-fast context switching. Header-only, zero dependencies, cross-platform.

[![CI](https://github.com/PavelGuzenfeld/ucoro/actions/workflows/ci.yml/badge.svg)](https://github.com/PavelGuzenfeld/ucoro/actions/workflows/ci.yml) [![Sanitizers](https://github.com/PavelGuzenfeld/ucoro/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/PavelGuzenfeld/ucoro/actions/workflows/sanitizers.yml) [![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23) [![License](https://img.shields.io/badge/license-MIT%2FUnlicense-green.svg)](LICENSE) [![Header Only](https://img.shields.io/badge/header--only-yes-brightgreen.svg)]() [![Platform](https://img.shields.io/badge/platform-windows%20%7C%20linux%20%7C%20macos-lightgrey.svg)]()

| Compiler    | Minimum Version |
| ----------- | --------------- |
| GCC         | 13+             |
| Clang       | 18+             |
| MSVC        | 2022 (19.38+)   |
| Apple Clang | 15+             |

## Features

- **~40ns context switches** - 10-19x faster than POSIX `ucontext`, competitive with Boost.Context
- **Modern C++23 API** - `std::expected`, concepts, strong types, `[[nodiscard]]`
- **Header-only, zero dependencies** - single header, no forced third-party libraries
- **Exception safe** - exceptions in coroutines are captured, not undefined behavior
- **Guard pages** - stack overflow triggers SIGSEGV/access violation instead of silent corruption
- **Zero-overhead abstractions** - safe API adds minimal overhead vs raw C; unchecked API adds none
- **Cross-platform** - Windows x64, Linux x64/ARM64, macOS x64/ARM64
- **Generators** - Python-style generators with range-for support
- **Task runner** - cooperative round-robin scheduler
- **Type-safe storage** - LIFO data passing between coroutine and caller
- **fmt support** - optional `fmt::formatter` specializations (auto-detected)

See the **[Roadmap](ROADMAP.md)** for planned features and release schedule.

## Quick Start

### Installation

**Option 1: CMake FetchContent**
```cmake
include(FetchContent)
FetchContent_Declare(ucoro
    GIT_REPOSITORY https://github.com/PavelGuzenfeld/ucoro.git
    GIT_TAG main
)
FetchContent_MakeAvailable(ucoro)

target_link_libraries(your_target PRIVATE ucoro::ucoro)
```

**Option 2: Copy the header**
```bash
# Copy include/ucoro/ucoro.hpp to your project — no other files needed
```

### Basic Usage

```cpp
// main.cpp
#define UCORO_IMPL  // Define in exactly ONE source file
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

for (int value : *fib) {
    std::printf("%d ", value);
    if (value > 100) break;
}
// Output: 0 1 1 2 3 5 8 13 21 34 55 89 144
```

### Data Passing (Storage)

```cpp
auto coro = coro::coroutine::create([](coro::coroutine_handle h) {
    auto value = h.pop<int>();
    if (value) {
        std::printf("received: %d\n", *value);
    }
});

(void)coro->push(42);
(void)coro->resume();
// Output: received: 42
```

### Task Runner (Cooperative Multitasking)

```cpp
coro::task_runner runner;

runner.add(std::move(*coro::coroutine::create([](coro::coroutine_handle h) {
    std::puts("task A: step 1");
    (void)h.yield();
    std::puts("task A: step 2");
})));

runner.add(std::move(*coro::coroutine::create([](coro::coroutine_handle h) {
    std::puts("task B: step 1");
    (void)h.yield();
    std::puts("task B: step 2");
})));

(void)runner.run();
// Output: task A: step 1, task B: step 1, task A: step 2, task B: step 2
```

### Exception Safety

Exceptions thrown inside a coroutine are captured and can be inspected by the caller:

```cpp
auto coro = coro::coroutine::create([](coro::coroutine_handle h) {
    (void)h.yield();  // first resume works fine
    throw std::runtime_error("something went wrong");
});

(void)coro->resume();  // step 1: OK
(void)coro->resume();  // step 2: coroutine throws, exception is captured

if (coro->has_exception()) {
    try {
        coro->rethrow_if_exception();
    } catch (std::exception const& e) {
        std::fprintf(stderr, "coroutine failed: %s\n", e.what());
    }
}
```

Without this, exceptions unwinding through assembly context-switch frames would be undefined behavior. ucoro catches them at the boundary and stores them for safe retrieval.

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

coro->push_unchecked(21);
coro->resume_unchecked();
int result = coro->pop_unchecked<int>(); // 42
```

## Advanced Examples

These examples demonstrate why you'd choose stackful coroutines over C++20's stackless `co_await`/`co_yield`.

### Deep Yield (Yield From Any Call Depth)

C++20 coroutines can only `co_yield` from the coroutine function itself. With stackful coroutines, you can yield from **any call depth** - no need to make every function in the chain async:

```cpp
void parse_nested_json(coro::coroutine_handle h, json_node const& node, int depth) {
    if (depth > max_depth) {
        h.yield_unchecked();  // Pause parsing, let other work run!
        return;
    }
    for (auto const& child : node.children()) {
        validate_node(child);
        parse_nested_json(h, child, depth + 1);  // Recursive - can still yield!
    }
}

auto json_worker = coro::coroutine::create([&](coro::coroutine_handle h) {
    process_large_file(h, massive_json_stream);
});

while (!json_worker.done()) {
    json_worker.resume_unchecked();
    handle_ui_events();  // UI never freezes
}
```

### Game AI Behavior (State Machines Made Readable)

```cpp
auto npc_brain = coro::coroutine::create([&](coro::coroutine_handle h) {
    while (npc.alive()) {
        // === PATROL ===
        for (auto const& waypoint : patrol_route) {
            while (!npc.at(waypoint)) {
                npc.move_toward(waypoint);
                h.yield_unchecked();  // Wait for next game tick
                if (npc.can_see(player)) goto chase;
            }
        }
        continue;

    chase:
        // === CHASE ===
        npc.yell("Stop right there!");
        while (npc.can_see(player) && npc.distance_to(player) > melee_range) {
            npc.sprint_toward(player.position());
            h.yield_unchecked();
        }
        // ... attack, search, etc.
    }
});

void game_update() {
    for (auto& npc : world.npcs)
        if (!npc.brain->done())
            npc.brain->resume_unchecked();
}
```

### Wrapping Callback-Based APIs

Turn callback spaghetti into linear async code:

```cpp
class async_socket {
    coro::coroutine* coro_;
    std::span<std::byte const> last_read_;
    std::error_code last_error_;

public:
    auto read(socket_t sock, std::span<std::byte> buffer)
        -> std::expected<std::span<std::byte const>, std::error_code>
    {
        async_read(sock, buffer, [this](auto data, auto ec) {
            last_read_ = data;
            last_error_ = ec;
            coro_->resume_unchecked();  // Callback resumes us
        });
        coro::running()->yield_unchecked();  // Suspend until callback fires
        if (last_error_) return std::unexpected(last_error_);
        return last_read_;
    }
};

auto handler = coro::coroutine::create([&](coro::coroutine_handle h) {
    async_socket sock{h};
    auto header = sock.read(client, buffer);  // Looks sync, is async
    auto body = sock.read(client, buffer);
    sock.write(client, generate_response(*header, body.value_or({})));
});
```

## API Reference

### Error Handling

All fallible operations return `std::expected<T, coro::error>`:

```cpp
enum class error : std::uint8_t {
    success, generic_error, invalid_pointer, invalid_coroutine,
    not_suspended, not_running, make_context_error, switch_context_error,
    not_enough_space, out_of_memory, invalid_arguments, invalid_operation,
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

### `coro::coroutine`

| Method | Description |
|--------|-------------|
| `create(func)` | Create a coroutine. Returns `std::expected<coroutine, error>` |
| `create(func, stack_size, storage_size)` | Create with custom sizes |
| `resume()` | Resume execution. Returns `std::expected<void, error>` |
| `resume_unchecked()` | Resume without checks (fastest path) |
| `done()` / `suspended()` / `is_running()` | Query state |
| `push<T>(value)` / `pop<T>()` / `peek<T>()` | Type-safe storage (LIFO) |
| `push_unchecked<T>()` / `pop_unchecked<T>()` | Storage without checks |
| `has_exception()` | Check if coroutine threw an exception |
| `exception()` | Get the `std::exception_ptr` |
| `rethrow_if_exception()` | Rethrow the captured exception |

### `coro::generator<T>`

| Method | Description |
|--------|-------------|
| `create(func)` | Create a generator. Returns `std::expected<generator, error>` |
| `next()` | Get next value. Returns `std::expected<std::optional<T>, error>` |
| `begin()` / `end()` | Range-for support via `std::default_sentinel` |
| `done()` | Check if generator is exhausted |

### `coro::task_runner`

| Method | Description |
|--------|-------------|
| `add(coroutine&&)` | Add a task to the scheduler |
| `run()` | Run all tasks to completion (round-robin) |
| `step()` | Execute one round of all tasks. Returns `true` if tasks remain |
| `size()` / `empty()` | Query task count |

### Configuration

```cpp
// Compile-time (define before including ucoro.hpp)
#define UCORO_STACK_SIZE     (56 * 1024)  // Default coroutine stack size
#define UCORO_MIN_STACK_SIZE  32768       // Minimum allowed stack
#define UCORO_STORAGE_SIZE    1024        // Default storage for push/pop
#define UCORO_GUARD_PAGES     1           // Enable stack guard pages (default: on)

// Runtime (per-coroutine)
auto coro = coro::coroutine::create(func,
    coro::stack_size{128 * 1024},
    coro::storage_size{4096}
);
```

### Concepts

```cpp
// Types that can be pushed/popped through coroutine storage
template <typename T>
concept storable = std::is_trivially_copyable_v<T>
               && std::is_standard_layout_v<T>
               && (sizeof(T) <= UCORO_STORAGE_SIZE);
```

### fmt Support (Optional)

ucoro has **no dependency on fmt**. However, if you include `<fmt/core.h>` before `<ucoro/ucoro.hpp>`, formatters for `coro::error` and `coro::state` are automatically enabled:

```cpp
#include <fmt/core.h>    // Include fmt first
#include <ucoro/ucoro.hpp>  // Detects FMT_VERSION, enables formatters

fmt::println("state: {}", coro.status());     // "state: suspended"
fmt::println("error: {}", result.error());    // "error: invalid arguments"
```

## Safety

### Exception Safety

Exceptions thrown inside a coroutine cannot propagate through assembly context-switch frames (that would be undefined behavior). ucoro catches exceptions at the coroutine boundary and stores them via `std::exception_ptr`. The caller can inspect or rethrow them after `resume()` returns.

The unchecked API (`resume_unchecked()`) does **not** check for exceptions — use it only when you know the coroutine body won't throw.

### Guard Pages

By default on Linux, macOS, and Windows, ucoro allocates coroutine stacks using `mmap`/`VirtualAlloc` with a guard page between the metadata and the stack region. Stack overflow triggers a hardware fault (SIGSEGV/access violation) instead of silently corrupting adjacent memory.

Disable with `#define UCORO_GUARD_PAGES 0` if needed (embedded systems, custom allocators).

### Stack Overflow Detection

In addition to guard pages, the safe `yield()` path checks the current stack pointer against the coroutine's stack bounds and validates a magic number. This catches overflows at yield points even without guard pages.

## Benchmarks

All numbers from Release builds with LTO enabled.

### Context Switch Latency (median, lower is better)

| Platform                      | ucoro Safe | ucoro Unchecked | Boost.Context | ucontext | Speedup vs ucontext |
| ----------------------------- | ---------- | --------------- | ------------- | -------- | ------------------- |
| **Linux x64** (GCC 13)        | 55 ns      | **52 ns**       | 29 ns         | 499 ns   | **~10x**            |
| **Windows x64** (MSVC, CI)    | 100 ns     | 100 ns          | N/A           | N/A      | -                   |
| **macOS ARM64** (CI)          | 42 ns      | 42 ns           | N/A           | 1,625 ns | **~39x**            |
| **Ubuntu x64** (Clang 18, CI) | 40 ns      | 40 ns           | N/A           | 652 ns   | **~16x**            |

### Context Switch Throughput (ops/sec, higher is better)

| Platform                      | ucoro Safe | ucoro Unchecked | Boost.Context | ucontext |
| ----------------------------- | ---------- | --------------- | ------------- | -------- |
| **Linux x64** (GCC 13)        | 15.0M      | **16.5M**      | 30.6M         | 1.7M     |
| **Windows x64** (CI)          | 18.1M      | 18.2M           | N/A           | N/A      |
| **macOS ARM64** (CI)          | 30.3M      | 30.8M           | N/A           | 611K     |
| **Ubuntu x64** (Clang 18, CI) | 23.2M      | 22.1M           | N/A           | 1.51M    |

### Memory Overhead

| Type                     | Size      |
| ------------------------ | --------- |
| `coro::coroutine`        | 24 bytes  |
| `coro::coroutine_handle` | 8 bytes   |
| `coro::task_runner`      | 24 bytes  |
| Internal `mco_coro`      | 136 bytes |
| Default stack            | 56 KB     |
| Default storage          | 1 KB      |
| Guard page overhead      | ~8 KB     |

## Building

### Requirements

- C++23 compiler (GCC 13+, Clang 18+, MSVC 2022+)
- CMake 3.22+
- [fmt](https://github.com/fmtlib/fmt) (only for tests/benchmarks/examples; fetched automatically)

### Build Commands

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### CMake Options

| Option                    | Default | Description                |
| ------------------------- | ------- | -------------------------- |
| `UCORO_BUILD_TESTS`       | `ON`    | Build test suite           |
| `UCORO_BUILD_BENCHMARKS`  | `ON`    | Build benchmarks           |
| `UCORO_BUILD_EXAMPLES`    | `ON`    | Build examples             |
| `UCORO_ENABLE_SANITIZERS` | `ON`    | Enable ASan/UBSan in Debug |

## Platform Support

| Platform | Architecture          | Compiler           | Status          |
| -------- | --------------------- | ------------------ | --------------- |
| Linux    | x86_64                | GCC 13+, Clang 18+ | Tested in CI    |
| Linux    | ARM64                 | GCC 13+, Clang 18+ | Supported       |
| macOS    | ARM64 (Apple Silicon) | Apple Clang 15+    | Tested in CI    |
| macOS    | x86_64                | Apple Clang 15+    | Supported       |
| Windows  | x64                   | MSVC 2022+         | Tested in CI    |

## How It Works

ucoro uses hand-written assembly for context switching on each platform:

- **x86_64**: Saves/restores RBP, RBX, R12-R15, RSP, RIP (8 registers, 64 bytes)
- **ARM64**: Saves/restores X19-X30, SP, LR, D8-D15 (callee-saved per AAPCS64)
- **Windows x64**: Additionally saves XMM6-XMM15 and TEB fiber storage fields

The assembly is embedded directly in the header via `__asm__` blocks (Unix) or raw byte arrays (Windows), requiring no external assembler or build step.

## Thread Safety

- Each coroutine must only be accessed from one thread at a time
- `coro::running()` is thread-local - safe to call from any thread
- `task_runner` is not thread-safe - use one per thread
- Creating and destroying coroutines from different threads is safe

## Why Not C++20 Coroutines?

C++20 coroutines are **stackless** - they can only suspend at explicit `co_await`/`co_yield` points. ucoro provides **stackful** coroutines that can suspend from any call depth:

- Yield from deep recursion or library code without making every function async
- Wrap legacy callback-based APIs as linear code
- Implement green threads, fibers, game AI behavior trees
- No viral `async`/`await` propagation

## License

MIT OR Unlicense (your choice)

Based on [minicoro](https://github.com/edubart/minicoro) by Eduardo Bart.
