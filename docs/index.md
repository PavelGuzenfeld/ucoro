# ucoro

Stackful coroutines for C++23 in one header, built on minicoro.

## Features

- **Context switch**: 6.4 ns per round trip on Linux x64. Boost.Context takes 5.2 ns, POSIX `ucontext` 382 ns ([Benchmarks](benchmarks.md))
- **C++23 API**: `std::expected`, concepts, strong types, `[[nodiscard]]`
- **One header, no dependencies**: fmt formatters turn on only if you include fmt first
- **Exceptions**: caught at the coroutine boundary and handed back to the caller
- **Guard pages**: a stack overflow faults instead of corrupting memory
- **One allocation**: metadata, storage, stack and the `std::function` share one block. A capture too big for `std::function`'s small buffer allocates once more
- **Checked and unchecked calls**: the checks cost about 0.7 ns per round trip
- **Platforms**: CI covers Windows x64, Linux x64 and macOS ARM64. Linux ARM64 and macOS x64 are implemented but untested
- **Generators**: `generator<T>` works in range-for
- **Task runner**: cooperative round-robin
- **Storage**: typed LIFO stack for passing values between coroutine and caller

[Getting started](getting-started.md) · [Source](https://github.com/PavelGuzenfeld/ucoro) · [Releases](https://github.com/PavelGuzenfeld/ucoro/releases)
