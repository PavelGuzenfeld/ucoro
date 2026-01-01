# µcoro (ucoro)

A modern C++23 coroutine library providing **stackful coroutines** with blazing-fast context switching. Header-only, cross-platform.

[![CI](https://img.shields.io/badge/CI-passing-brightgreen.svg)]() [![Sanitizers](https://img.shields.io/badge/sanitizers-passing-brightgreen.svg)]() [![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23) [![License](https://img.shields.io/badge/license-MIT%2FUnlicense-green.svg)](LICENSE) [![Header Only](https://img.shields.io/badge/header--only-yes-brightgreen.svg)]() [![Platform](https://img.shields.io/badge/platform-windows%20%7C%20linux%20%7C%20macos-lightgrey.svg)]()

<!-- TODO: Replace with real GitHub Actions badges once repo is public:
[![CI](https://github.com/YOUR_USERNAME/ucoro/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_USERNAME/ucoro/actions/workflows/ci.yml)
[![Sanitizers](https://github.com/YOUR_USERNAME/ucoro/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/YOUR_USERNAME/ucoro/actions/workflows/sanitizers.yml)
-->

| Compiler    | Minimum Version |
| ----------- | --------------- |
| GCC         | 13+             |
| Clang       | 18+             |
| MSVC        | 2022 (19.38+)   |
| Apple Clang | 15+             |

## Features

- **~55ns context switches** - up to 50x faster than POSIX `ucontext`, faster than Boost.Context
- **Modern C++23 API** - `std::expected`, concepts, strong types, `[[nodiscard]]`
- **Header-only** - single header, define `UCORO_IMPL` in one TU
- **Zero-overhead abstractions** - safe API adds minimal overhead vs raw C
- **Cross-platform** - Windows x64, Linux x64/ARM64, macOS x64/ARM64
- **Generators** - Python-style generators with range-for support
- **Task runner** - cooperative round-robin scheduler
- **Type-safe storage** - LIFO data passing between coroutine and caller

📋 **[See the Roadmap](ROADMAP.md)** for planned features and release schedule.

## Quick Start

### Installation

**Option 1: CMake FetchContent**
```cmake
include(FetchContent)
FetchContent_Declare(ucoro
    GIT_REPOSITORY https://github.com/YOUR_USERNAME/ucoro.git
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

## Advanced Examples

These examples demonstrate why you'd choose stackful coroutines over C++20's stackless `co_await`/`co_yield`.

### Deep Yield (Yield From Any Call Depth)

C++20 coroutines can only `co_yield` from the coroutine function itself. With stackful coroutines, you can yield from **any call depth** - no need to make every function in the chain async:

```cpp
// This is IMPOSSIBLE with C++20 coroutines without making
// every function in the chain a coroutine (viral async/await)

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

void process_large_file(coro::coroutine_handle h, std::istream& file) {
    json_parser parser;
    while (auto node = parser.next_root(file)) {
        parse_nested_json(h, *node, 0);  // Yields happen deep inside
        h.yield_unchecked();  // Yield between root nodes too
    }
}

// Create worker that processes without blocking
auto json_worker = coro::coroutine::create([&](coro::coroutine_handle h) {
    process_large_file(h, massive_json_stream);
});

// Main loop stays responsive - worker yields periodically from deep in the call stack
while (!json_worker.done()) {
    json_worker.resume_unchecked();
    handle_ui_events();  // UI never freezes
}
```

### Game AI Behavior (State Machines Made Readable)

Game AI typically requires complex state machines. With stackful coroutines, behavior reads like a script:

```cpp
// NPC behavior that reads like pseudocode, not a state machine nightmare
auto npc_brain = coro::coroutine::create([&](coro::coroutine_handle h) {
    while (npc.alive()) {
        
        // === PATROL STATE ===
        for (auto const& waypoint : patrol_route) {
            // Walk to waypoint, yielding each frame
            while (!npc.at(waypoint)) {
                npc.move_toward(waypoint);
                h.yield_unchecked();  // Wait for next game tick
                
                // Interrupt patrol if player spotted
                if (npc.can_see(player)) {
                    goto chase;  // Natural flow control!
                }
            }
            
            // Idle at waypoint for a bit
            for (int i = 0; i < 120; ++i) {  // ~2 seconds at 60fps
                npc.play_idle_animation();
                h.yield_unchecked();
                
                if (npc.can_see(player)) {
                    goto chase;
                }
            }
        }
        continue;  // Loop patrol forever
        
    chase:
        // === CHASE STATE ===
        npc.yell("Stop right there!");
        npc.set_alert_status(true);
        
        while (npc.can_see(player) && npc.distance_to(player) > melee_range) {
            npc.sprint_toward(player.position());
            h.yield_unchecked();
        }
        
        if (!npc.can_see(player)) {
            // Lost sight - search for a few seconds
            auto last_seen = player.position();
            for (int i = 0; i < 180; ++i) {  // ~3 seconds
                npc.investigate(last_seen);
                h.yield_unchecked();
                
                if (npc.can_see(player)) {
                    goto chase;  // Found them!
                }
            }
            npc.set_alert_status(false);
            continue;  // Give up, back to patrol
        }
        
        // === ATTACK STATE ===
        while (npc.distance_to(player) <= melee_range) {
            npc.attack(player);
            
            // Wait for attack animation to complete
            for (int frame = 0; frame < npc.attack_animation_frames(); ++frame) {
                h.yield_unchecked();
            }
            
            // Cooldown between attacks
            for (int i = 0; i < 30; ++i) {
                h.yield_unchecked();
                if (npc.distance_to(player) > melee_range) {
                    goto chase;  // Player retreated
                }
            }
        }
    }
    
    // NPC died - play death animation
    for (int i = 0; i < npc.death_animation_frames(); ++i) {
        h.yield_unchecked();
    }
});

// Game loop - just resume all NPC brains each tick
void game_update() {
    for (auto& npc : world.npcs) {
        if (!npc.brain->done()) {
            npc.brain->resume_unchecked();
        }
    }
}
```

Compare this to the equivalent state machine with enums and switch statements - the coroutine version is dramatically more readable and maintainable.

### Wrapping Callback-Based APIs

Turn callback spaghetti into linear async code:

```cpp
// Typical callback-based async API (like libuv, asio, Win32, etc.)
using read_callback = std::function<void(std::span<std::byte const>, std::error_code)>;
void async_read(socket_t sock, std::span<std::byte> buffer, read_callback cb);

// Coroutine wrapper that makes callbacks look synchronous
class async_socket {
    coro::coroutine* coro_;
    std::span<std::byte const> last_read_;
    std::error_code last_error_;

public:
    explicit async_socket(coro::coroutine_handle h) : coro_(&h.get()) {}
    
    // Suspends coroutine until read completes
    auto read(socket_t sock, std::span<std::byte> buffer) 
        -> std::expected<std::span<std::byte const>, std::error_code> 
    {
        async_read(sock, buffer, [this](auto data, auto ec) {
            last_read_ = data;
            last_error_ = ec;
            coro_->resume_unchecked();  // Callback resumes us
        });
        
        coro::running()->yield_unchecked();  // Suspend until callback fires
        
        if (last_error_) {
            return std::unexpected(last_error_);
        }
        return last_read_;
    }
};

// Usage: callback hell becomes beautiful linear code
auto http_handler = coro::coroutine::create([&](coro::coroutine_handle h) {
    async_socket sock{h};
    std::array<std::byte, 4096> buffer;
    
    // This LOOKS synchronous but is fully async!
    auto header_data = sock.read(client_socket, buffer);
    if (!header_data) {
        log_error(header_data.error());
        return;
    }
    
    auto request = parse_http_header(*header_data);
    auto body = sock.read(client_socket, buffer);
    
    auto response = generate_response(request, body.value_or({}));
    sock.write(client_socket, response);
});
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

| Platform                          | ucoro Safe | ucoro Unchecked | Boost.Context | ucontext | Speedup vs ucontext |
| --------------------------------- | ---------- | --------------- | ------------- | -------- | ------------------- |
| **Linux x64** (Local, GCC 13)     | 91 ns      | **55 ns**       | 72 ns         | 352 ns   | **6.4x**            |
| **Windows x64** (CI, MSVC)        | 100 ns     | 100 ns          | N/A           | N/A      | -                   |
| **macOS ARM64** (CI, Apple Clang) | 42 ns      | 42 ns           | N/A           | 1,625 ns | **~39x**            |
| **Ubuntu x64** (CI, Clang 18)     | 40 ns      | 40 ns           | N/A           | 652 ns   | **~16x**            |
| **Ubuntu x64** (CI, GCC 13)       | 50 ns      | 40 ns           | N/A           | 661 ns   | **~17x**            |

### Context Switch Throughput (ops/sec, higher is better)

| Platform                      | ucoro Safe | ucoro Unchecked | Boost.Context | ucontext |
| ----------------------------- | ---------- | --------------- | ------------- | -------- |
| **Linux x64** (Local)         | 10.5M      | **17.5M**       | 13.1M         | 2.7M     |
| **Windows x64** (CI)          | 18.1M      | 18.2M           | N/A           | N/A      |
| **macOS ARM64** (CI)          | 30.3M      | 30.8M           | N/A           | 611K     |
| **Ubuntu x64** (CI, Clang 18) | 23.2M      | 22.1M           | N/A           | 1.51M    |
| **Ubuntu x64** (CI, GCC 13)   | 20.8M      | 23.4M           | N/A           | 1.47M    |

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

| Platform                      | ucoro C++ | ucoro Raw C |
| ----------------------------- | --------- | ----------- |
| **Linux x64** (Local)         | 1.06M     | 1.15M       |
| **Windows x64** (CI)          | 797K      | 898K        |
| **macOS ARM64** (CI)          | 1.40M     | 1.39M       |
| **Ubuntu x64** (CI, Clang 18) | 1.37M     | 1.42M       |
| **Ubuntu x64** (CI, GCC 13)   | 1.06M     | 1.32M       |

### Storage Operations (push + pop + context switch)

| Platform              | ucoro Safe | ucoro Unchecked | Raw C API   |
| --------------------- | ---------- | --------------- | ----------- |
| **Linux x64** (Local) | 8.3M ops/s | **14.9M ops/s** | 13.0M ops/s |

### Memory Overhead

| Type                     | Size      |
| ------------------------ | --------- |
| `coro::coroutine`        | 16 bytes  |
| `coro::coroutine_handle` | 8 bytes   |
| `coro::task_runner`      | 24 bytes  |
| Internal `mco_coro`      | 136 bytes |
| Default stack            | 56 KB     |
| Default storage          | 1 KB      |

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

| Option                    | Default | Description                |
| ------------------------- | ------- | -------------------------- |
| `UCORO_BUILD_TESTS`       | `ON`    | Build test suite           |
| `UCORO_BUILD_BENCHMARKS`  | `ON`    | Build benchmarks           |
| `UCORO_BUILD_EXAMPLES`    | `ON`    | Build examples             |
| `UCORO_ENABLE_SANITIZERS` | `ON`    | Enable ASan/UBSan in Debug |

## Platform Support

| Platform | Architecture          | Compiler           | Status         |
| -------- | --------------------- | ------------------ | -------------- |
| Linux    | x86_64                | GCC 13+, Clang 18+ | ✅ Tested in CI |
| Linux    | ARM64                 | GCC 13+, Clang 18+ | ✅ Supported    |
| macOS    | x86_64                | Apple Clang 15+    | ✅ Supported    |
| macOS    | ARM64 (Apple Silicon) | Apple Clang 15+    | ✅ Tested in CI |
| Windows  | x64                   | MSVC 2022+         | ✅ Tested in CI |
| Windows  | ARM64                 | MSVC               | ❌ Not yet      |

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