# API reference

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
| `step()` | Execute one round of all tasks. Returns `std::expected<bool, error>`; `true` if tasks remain |
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

If `<fmt/core.h>` is included before `<ucoro/ucoro.hpp>`, formatters for `coro::error` and `coro::state` are enabled:

```cpp
#include <fmt/core.h>    // Include fmt first
#include <ucoro/ucoro.hpp>  // Detects FMT_VERSION, enables formatters

fmt::println("state: {}", coro.status());     // "state: suspended"
fmt::println("error: {}", result.error());    // "error: invalid arguments"
```
