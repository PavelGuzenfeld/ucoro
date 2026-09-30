# ucoro

Stackful coroutines for C++23 in one header, built on minicoro.

Status: maintenance only. Bug fixes and platform fixes are accepted.

## Features

- **Fast context switches** - 6.4 ns per round trip on Linux x64, within about 25% of Boost.Context and about 60x faster than POSIX `ucontext`
- **C++23 API** - `std::expected`, concepts, strong types, `[[nodiscard]]`
- **Header-only, no dependencies** - one header; fmt formatters only if you include fmt
- **Exception safe** - exceptions in coroutines are captured, not undefined behavior
- **Guard pages** - stack overflow triggers SIGSEGV/access violation instead of silent corruption
- **Single-allocation design** - the `std::function` object, metadata, storage, and stack share one contiguous block; a capture larger than `std::function`'s small buffer still allocates once
- **Checked and unchecked APIs** - the checked path costs about 0.7 ns per round trip on Linux x64
- **Cross-platform** - CI-tested on Windows x64, Linux x64 and macOS ARM64; Linux ARM64 and macOS x64 are implemented but not CI-tested
- **Generators** - `generator<T>` works in range-for
- **Task runner** - cooperative round-robin scheduler
- **Type-safe storage** - LIFO data passing between coroutine and caller
- **fmt support** - optional `fmt::formatter` specializations (auto-detected)

[Getting started](getting-started.md) · [Source](https://github.com/PavelGuzenfeld/ucoro) · [Releases](https://github.com/PavelGuzenfeld/ucoro/releases)
