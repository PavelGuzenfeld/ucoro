# Safety

## Exceptions

An exception can't propagate through the assembly switch; that would be undefined behavior. ucoro catches it at the coroutine boundary and keeps it as a `std::exception_ptr`, which the caller can inspect or rethrow after `resume()` returns.

`resume_unchecked()` does not check for a captured exception. Use it only when the body can't throw.

## Guard pages

On Linux, macOS and Windows, stacks come from `mmap`/`VirtualAlloc` with a guard page between the metadata and the stack. An overflow faults (SIGSEGV or an access violation) instead of corrupting the memory next to it.

`#define UCORO_GUARD_PAGES 0` turns this off, for embedded targets or custom allocators.

## Overflow checks

The checked `yield()` also compares the stack pointer against the coroutine's stack bounds and checks a magic number, so an overflow is caught at the next yield even without guard pages.
