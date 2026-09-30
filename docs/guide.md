# Guide

## Generators

```cpp
auto fib = coro::generator<int>::create([](coro::coroutine_handle h) {
    int a = 0, b = 1;
    while (true) {
        (void)coro::yield_value(h, a);
        int next = a + b;
        a = b;
        b = next;
    }
});

for (int value : *fib) {
    std::printf("%d ", value);
    if (value > 100) break;
}
// Output: 0 1 1 2 3 5 8 13 21 34 55 89 144
```

## Data Passing (Storage)

```cpp
auto coro = coro::coroutine::create([](coro::coroutine_handle h) {
    auto value = h.pop<int>();
    if (value) {
        std::printf("received: %d\n", *value);
    }
});

(void)coro->push(42);
(void)coro->resume();
// Output: received: 42
```

## Task Runner

```cpp
coro::task_runner runner;

runner.add(std::move(*coro::coroutine::create([](coro::coroutine_handle h) {
    std::puts("task A: step 1");
    (void)h.yield();
    std::puts("task A: step 2");
})));

runner.add(std::move(*coro::coroutine::create([](coro::coroutine_handle h) {
    std::puts("task B: step 1");
    (void)h.yield();
    std::puts("task B: step 2");
})));

(void)runner.run();
// Output: task A: step 1, task B: step 1, task A: step 2, task B: step 2
```

## Exception Safety

Exceptions thrown inside a coroutine are captured and can be inspected by the caller:

```cpp
auto coro = coro::coroutine::create([](coro::coroutine_handle h) {
    (void)h.yield();  // first resume works fine
    throw std::runtime_error("something went wrong");
});

(void)coro->resume();  // step 1: OK
(void)coro->resume();  // step 2: coroutine throws, exception is captured

if (coro->has_exception()) {
    try {
        coro->rethrow_if_exception();
    } catch (std::exception const& e) {
        std::fprintf(stderr, "coroutine failed: %s\n", e.what());
    }
}
```

Without this, exceptions unwinding through assembly context-switch frames would be undefined behavior. ucoro catches them at the boundary and stores them for safe retrieval.

## Unchecked API

For hot paths where you've already validated state:

```cpp
auto coro = coro::coroutine::create([](coro::coroutine_handle h) {
    while (true) {
        int val = h.pop_unchecked<int>();
        h.push_unchecked(val * 2);
        h.yield_unchecked();
    }
});

coro->push_unchecked(21);
coro->resume_unchecked();
int result = coro->pop_unchecked<int>(); // 42
```
