# Design

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
