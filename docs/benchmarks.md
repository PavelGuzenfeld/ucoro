# Benchmarks

A round trip is one resume plus its yield. `benchmark_ucoro` times 100 batches per case and reports the median batch divided by its size, so the clock cost is spread over a batch rather than added to every switch.

## Context switch round trip

Linux x64, Intel i7-12700H, pinned to one P-core, GCC 14 `-O3` with LTO, Boost 1.83. Median of three runs, measured 2026-10-01.

| Implementation  | ns per round trip |
| --------------- | ----------------- |
| ucoro raw C API | 5.6               |
| ucoro unchecked | 6.4               |
| ucoro safe      | 7.1               |
| Boost.Context   | 5.2               |
| POSIX ucontext  | 382               |

### Windows x64

MSVC `/O2` under Wine 9.0 on the same CPU, not pinned. Wine emulates only the Windows API, so the switch itself runs natively on the CPU, but this is not a measurement from a Windows machine. Median of six runs, measured 2026-10-01. Boost.Context was not built for this run.

| Implementation  | Before #15 | Current |
| --------------- | ---------- | ------- |
| ucoro unchecked | 30.0       | 18.3    |
| ucoro safe      | 35.5       | 18.2    |

The Windows switch also saves XMM6-XMM15 and the thread-block fields, so its floor is higher than on Linux.

ARM64 has not been measured with this harness.

## Memory

| Type                     | Size      |
| ------------------------ | --------- |
| `coro::coroutine`        | 24 bytes  |
| `coro::coroutine_handle` | 8 bytes   |
| `coro::task_runner`      | 24 bytes  |
| Internal `mco_coro`      | 152 bytes |
| Default stack            | 56 KB     |
| Default storage          | 1 KB      |
| Guard page overhead      | ~8 KB     |
