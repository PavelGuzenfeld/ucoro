# Advanced examples

Cases where a stackful coroutine does what C++20's stackless `co_await`/`co_yield` cannot.

## Yield from any call depth

C++20 coroutines can only `co_yield` from the coroutine function itself. A stackful coroutine yields from any call depth, so the functions in between stay ordinary:

```cpp
void parse_nested_json(coro::coroutine_handle h, json_node const& node, int depth) {
    if (depth > max_depth) {
        h.yield_unchecked();
        return;
    }
    for (auto const& child : node.children()) {
        validate_node(child);
        parse_nested_json(h, child, depth + 1);
    }
}

auto json_worker = coro::coroutine::create([&](coro::coroutine_handle h) {
    process_large_file(h, massive_json_stream);
});

while (!json_worker->done()) {
    json_worker->resume_unchecked();
    handle_ui_events();
}
```

## Game AI state machine

```cpp
auto npc_brain = coro::coroutine::create([&](coro::coroutine_handle h) {
    while (npc.alive()) {
        for (auto const& waypoint : patrol_route) {
            while (!npc.at(waypoint)) {
                npc.move_toward(waypoint);
                h.yield_unchecked();  // next game tick
                if (npc.can_see(player)) goto chase;
            }
        }
        continue;

    chase:
        npc.yell("Stop right there!");
        while (npc.can_see(player) && npc.distance_to(player) > melee_range) {
            npc.sprint_toward(player.position());
            h.yield_unchecked();
        }
        // ... attack, search, etc.
    }
});

void game_update() {
    for (auto& npc : world.npcs)
        if (!npc.brain->done())
            npc.brain->resume_unchecked();
}
```

## Wrapping a callback API

A callback-based read becomes a linear call:

```cpp
class async_socket {
    coro::coroutine_handle h_;
    coro::coroutine* self_;
    std::span<std::byte const> last_read_;
    std::error_code last_error_;

public:
    async_socket(coro::coroutine_handle h, coro::coroutine* self) : h_{h}, self_{self} {}

    auto read(socket_t sock, std::span<std::byte> buffer)
        -> std::expected<std::span<std::byte const>, std::error_code>
    {
        async_read(sock, buffer, [this](auto data, auto ec) {
            last_read_ = data;
            last_error_ = ec;
            self_->resume_unchecked();  // Callback resumes us
        });
        h_.yield_unchecked();  // Suspend until callback fires
        if (last_error_) return std::unexpected(last_error_);
        return last_read_;
    }
};

coro::coroutine* self = nullptr;
auto handler = coro::coroutine::create([&](coro::coroutine_handle h) {
    async_socket sock{h, self};
    auto header = sock.read(client, buffer);
    auto body = sock.read(client, buffer);
    sock.write(client, generate_response(*header, body.value_or(std::span<std::byte const>{})));
});
self = &*handler;
```
