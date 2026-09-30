# Benchmarks

One round trip is a resume plus the matching yield. `benchmark_ucoro` times 100 batches per case and reports the median batch divided by its size, so clock overhead is spread over the batch instead of added to every switch.

## Context switch round trip

Linux x64, Intel i7-12700H, pinned to one P-core, GCC 14 `-O3` with LTO, Boost 1.83. Median of three runs, measured 2026-10-01.

| Implementation  | ns per round trip |
| --------------- | ----------------- |
| ucoro raw C API | 5.6               |
| ucoro unchecked | 6.4               |
| ucoro safe      | 7.1               |
| Boost.Context   | 5.2               |
| POSIX ucontext  | 382               |

Windows x64 and ARM64 have not been measured with this harness.

## Memory Overhead

| Type                     | Size      |
| ------------------------ | --------- |
| `coro::coroutine`        | 24 bytes  |
| `coro::coroutine_handle` | 8 bytes   |
| `coro::task_runner`      | 24 bytes  |
| Internal `mco_coro`      | 152 bytes |
| Default stack            | 56 KB     |
| Default storage          | 1 KB      |
| Guard page overhead      | ~8 KB     |
