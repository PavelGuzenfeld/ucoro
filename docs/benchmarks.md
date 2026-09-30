# Benchmarks

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
| Internal `mco_coro`      | 152 bytes |
| Default stack            | 56 KB     |
| Default storage          | 1 KB      |
| Guard page overhead      | ~8 KB     |
