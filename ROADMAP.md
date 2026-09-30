# Roadmap

ucoro is maintenance-only. Bug fixes and platform fixes are accepted; no new features are scheduled.

## Released

### 0.1.0

- Upstream credit corrected to minicoro; licensing aligned on MIT
- README claims corrected to match the benchmark table, `sizeof`, and CI coverage
- FetchContent pinned to a release tag

### 0.0.1

- C++23 API over minicoro: `std::expected`, concepts, `[[nodiscard]]`, strong types (`stack_size`, `storage_size`)
- Header-only; define `UCORO_IMPL` in one translation unit; fmt formatters only when fmt is included first
- `coro::coroutine`, `coro::coroutine_handle`, `coro::generator<T>`, `coro::task_runner`
- Type-safe LIFO storage (`push`/`pop`/`peek`) constrained by the `storable` concept
- `*_unchecked()` variants for hot paths
- Exceptions captured at the context-switch boundary; `has_exception()`, `exception()`, `rethrow_if_exception()`
- Guard pages via `mmap`/`VirtualAlloc`, plus stack-bounds and magic-number checks on `yield()`
- 52 unit tests (280 assertions) and 17 integration tests
- CI: GCC 13 and Clang 18 on Linux x64, Apple Clang on macOS ARM64, MSVC on Windows x64, ASan + UBSan

## Not planned

Open to a PR with an issue first:

- CI coverage for Linux ARM64 and macOS x64, and a Debug build matrix
- clang-format / clang-tidy with pre-commit
- Symmetric transfer (`coro.transfer(other)`)
- Coroutine pools
- Cancellation tokens
- Windows ARM64, RISC-V
- vcpkg / Conan packages

## Non-goals

- Thread safety: a coroutine belongs to one thread; use one `task_runner` per thread
- Preemption: scheduling is cooperative
- Interop with C++20 stackless coroutines
- Growable stacks: stacks are fixed size
