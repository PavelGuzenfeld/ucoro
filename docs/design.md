# Design

## Context switch

Each platform has its own hand-written switch:

- **x86_64**: Saves/restores RBP, RBX, R12-R15, RSP, RIP (8 registers, 64 bytes)
- **ARM64**: Saves/restores X19-X30, SP, LR, D8-D15 (callee-saved per AAPCS64)
- **Windows x64**: Additionally saves XMM6-XMM15 and TEB fiber storage fields

The assembly lives in the header, as `__asm__` blocks on Unix and byte arrays on Windows, so there is no separate assembler step.

## Threads

- Each coroutine must only be accessed from one thread at a time
- `coro::running()` is thread-local - safe to call from any thread
- `task_runner` is not thread-safe - use one per thread
- Creating and destroying coroutines from different threads is safe

## Why not C++20 coroutines

C++20 coroutines are stackless: they suspend only at a `co_await` or `co_yield` in the coroutine body, and every caller on the way has to be a coroutine too. A ucoro coroutine has its own stack, so any function it calls can yield. That makes it a fit for deep recursion, wrapping callback APIs, fibers and game logic. See [Advanced examples](examples.md).

