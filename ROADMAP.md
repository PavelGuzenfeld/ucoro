# µcoro Roadmap

Current version: **0.0.1** (initial release)

## Version 0.0.1 — Initial Release ✓

### Core Features (Complete)
- [x] ~55ns context switches (up to 50x faster than POSIX `ucontext`, faster than Boost.Context)
- [x] Modern C++23 API (`std::expected`, concepts, strong types, `[[nodiscard]]`)
- [x] Header-only distribution (single header, define `UCORO_IMPL` in one TU)
- [x] Zero-overhead abstractions (safe API adds minimal overhead vs raw C)
- [x] Cross-platform support (Windows x64, Linux x64/ARM64, macOS x64/ARM64)

### API (Complete)
- [x] `coro::coroutine` — stackful coroutine with RAII semantics
- [x] `coro::coroutine_handle` — lightweight non-owning handle for use inside coroutines
- [x] `coro::generator<T>` — Python-style generators with range-for support
- [x] `coro::task_runner` — cooperative round-robin scheduler
- [x] Type-safe storage — LIFO data passing between coroutine and caller (`push`/`pop`)
- [x] `storable` concept — compile-time validation for storage types
- [x] Strong types — `stack_size`, `storage_size` prevent parameter confusion
- [x] Safe API — all operations return `std::expected<T, coro::error>`
- [x] Unchecked API — `*_unchecked()` variants for hot paths

### Testing & CI (Complete)
- [x] Comprehensive test suite (45+ tests)
- [x] Benchmarks comparing against raw C API and POSIX `ucontext`
- [x] CI on Windows (MSVC), Linux (GCC, Clang), macOS (Apple Clang)
- [x] ASan + UBSan in debug builds

---

## Version 0.1.0 — Stabilization

### Code Quality
- [ ] Split header into maintainable sections (`detail/`, `fwd.hpp`, etc.)
- [ ] Make fmt dependency optional (only for tests/benchmarks/opt-in formatters)
- [ ] Add `UCORO_ASSERT` macro (user-overridable, defaults to assert)
- [ ] Improve error messages (static_assert with clear explanations)

### CI / Tooling
- [ ] Pre-commit hooks (`.pre-commit-config.yaml`)
  - [ ] clang-format (style enforcement)
  - [ ] clang-tidy (static analysis)
  - [ ] cppcheck (additional static analysis)
  - [ ] trailing whitespace / end-of-file fixers
  - [ ] YAML lint
- [ ] `.clang-format` config (project style)
- [ ] `.clang-tidy` config (enabled checks)
- [ ] CI runs pre-commit on all PRs (fail-fast)
- [ ] CI build matrix: GCC + Clang × Debug + Release
- [ ] CI sanitizer job (ASan + UBSan)
- [ ] Code coverage reporting (gcov/llvm-cov)
- [ ] Coverage threshold gate (e.g., 80% minimum)

### API Enhancements
- [ ] Coroutine naming for debugging (`coro::coroutine::create("worker", func)`)
- [ ] Stack usage introspection (`coro.stack_used()`, `coro.stack_remaining()`)

## Version 0.2.0 — Symmetric Transfers

- [ ] `coro.transfer(other)` — resume `other` without returning to caller
- [ ] Symmetric coroutine example (cooperative threading)
- [ ] Tail-call optimization for transfer chains

## Version 0.3.0 — Coroutine Pools

- [ ] `coro::pool` — pre-allocated coroutine pool
- [ ] Configurable pool size and stack size
- [ ] Zero-allocation resume/yield in steady state
- [ ] Pool statistics (active, idle, high watermark)

## Version 0.4.0 — Generator Combinators

- [ ] `generator::map(fn)` — transform yielded values
- [ ] `generator::filter(pred)` — skip values not matching predicate
- [ ] `generator::take(n)` — limit to first N values
- [ ] `generator::zip(other)` — combine two generators
- [ ] `generator::enumerate()` — yield (index, value) pairs
- [ ] `generator::flatten()` — flatten nested generators

## Version 0.5.0 — Cancellation & Timeouts

- [ ] `coro::cancellation_token` — cooperative cancellation
- [ ] `coro.cancel()` — request cancellation
- [ ] `h.cancellation_requested()` — check inside coroutine
- [ ] Timeout wrapper for task_runner

---

## Version 1.0.0 — Stable Release

- [ ] API freeze (semver guarantees)
- [ ] Comprehensive documentation
- [ ] vcpkg package
- [ ] Conan package
- [ ] Single-header amalgamation script

---

## Version 1.1.0 — Channels

- [ ] `coro::channel<T>` — bounded MPSC channel
- [ ] `channel.send(value)` — blocks if full
- [ ] `channel.receive(h)` — blocks if empty
- [ ] `channel.try_send()` / `channel.try_receive()` — non-blocking variants
- [ ] `channel.close()` — signal no more values

## Version 1.2.0 — Select / Multiplex

- [ ] `coro::select(channels...)` — wait on multiple channels
- [ ] `coro::select_any(coroutines...)` — resume when any completes
- [ ] Priority selection

## Version 1.3.0 — Structured Concurrency

- [ ] `coro::scope` — owns child coroutines, ensures cleanup
- [ ] Automatic cancellation of children when scope exits
- [ ] Exception propagation from children to parent
- [ ] `scope.spawn(fn)` — launch child coroutine

---

## Future / Experimental

### Platform Support
- [ ] Windows ARM64
- [ ] RISC-V (rv64gc)
- [ ] WebAssembly (pending [stack switching proposal](https://github.com/WebAssembly/stack-switching))
- [ ] LoongArch64

### I/O Integration
- [ ] `io_uring` integration (Linux 5.1+)
- [ ] IOCP integration (Windows)
- [ ] kqueue integration (macOS/BSD)
- [ ] Async file I/O example
- [ ] Async socket example

### Tooling
- [ ] Fuzz testing harness (libFuzzer / oss-fuzz integration)
- [ ] Mutation testing (mutate_cpp)
- [ ] Sanitizer-friendly mode (help ASan understand coroutine stacks)
- [ ] GDB/LLDB pretty printers for `coro::coroutine`, `coro::generator<T>`
- [ ] Tracy profiler integration
- [ ] Bloaty McBloatface binary size tracking in CI

### Advanced
- [ ] Custom stack allocators (`coro::stack_allocator` concept)
- [ ] Guard pages for stack overflow detection (optional, platform-specific)
- [ ] Coroutine serialization (checkpoint/restore) — research only

---

## Non-Goals

Things we explicitly won't do:

- **Thread safety** — coroutines are single-threaded by design. Use one task_runner per thread.
- **Preemption** — this is cooperative multitasking. If you want preemption, use OS threads.
- **C++20 coroutine interop** — stackful and stackless are fundamentally different. Pick one.
- **Dynamic stack growth** — stacks are fixed size. Allocate enough upfront.

---

## Files To Be Created (v0.1.0)

### CI Workflows
```
.github/workflows/
├── ci.yml              # Main build + test matrix
├── sanitizers.yml      # ASan/UBSan dedicated job  
├── coverage.yml        # Code coverage reporting
└── pre-commit.yml      # Lint check on PRs
```

### Tooling Configs
```
.clang-format           # Code style (BasedOnStyle: LLVM, customized)
.clang-tidy             # Static analysis checks
.pre-commit-config.yaml # Pre-commit hook definitions
cppcheck.suppressions   # False positive suppressions
```

### Header Structure
```
include/ucoro/
├── ucoro.hpp           # Main include (includes everything)
├── fwd.hpp             # Forward declarations
├── config.hpp          # Macros, defaults, platform detection  
├── error.hpp           # error enum, to_string
├── types.hpp           # stack_size, storage_size, state, storable concept
├── format.hpp          # fmt::formatter specializations (optional)
└── detail/
    ├── minicoro.h      # Raw C minicoro (unchanged)
    ├── coroutine.hpp   # coroutine class
    ├── handle.hpp      # coroutine_handle
    ├── generator.hpp   # generator<T>
    └── task_runner.hpp # task_runner
```

---

## Contributing

See an item you want? PRs welcome. Please open an issue first to discuss approach.

Priority is given to:
1. Bug fixes
2. Platform support
3. Performance improvements
4. New features (in roadmap order)