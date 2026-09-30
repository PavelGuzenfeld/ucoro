# Safety

### Exception Safety

Exceptions thrown inside a coroutine cannot propagate through assembly context-switch frames (that would be undefined behavior). ucoro catches exceptions at the coroutine boundary and stores them via `std::exception_ptr`. The caller can inspect or rethrow them after `resume()` returns.

The unchecked API (`resume_unchecked()`) does **not** check for exceptions — use it only when you know the coroutine body won't throw.

### Guard Pages

By default on Linux, macOS, and Windows, ucoro allocates coroutine stacks using `mmap`/`VirtualAlloc` with a guard page between the metadata and the stack region. Stack overflow triggers a hardware fault (SIGSEGV/access violation) instead of silently corrupting adjacent memory.

Disable with `#define UCORO_GUARD_PAGES 0` if needed (embedded systems, custom allocators).

### Stack Overflow Detection

In addition to guard pages, the safe `yield()` path checks the current stack pointer against the coroutine's stack bounds and validates a magic number. This catches overflows at yield points even without guard pages.
