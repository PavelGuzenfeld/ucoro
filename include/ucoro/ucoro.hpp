// ucoro.hpp - C++23 wrapper for ucoro
// Single-header implementation. Define UCORO_IMPL in *one* source file.
//
// SPDX-License-Identifier: MIT OR Unlicense
// Original C library: Eduardo Bart (https://github.com/edubart/ucoro)

#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib> // For calloc/free
#include <cstring> // For memcpy
#include <expected>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// ============================================================================
// Configuration (Externalizable via Build Parameters)
// ============================================================================

#ifndef UCORO_STACK_SIZE
#define UCORO_STACK_SIZE (56 * 1024)
#endif

#ifndef UCORO_MIN_STACK_SIZE
#define UCORO_MIN_STACK_SIZE 32768
#endif

#ifndef UCORO_STORAGE_SIZE
#define UCORO_STORAGE_SIZE 1024
#endif

namespace coro
{
    // ============================================================================
    // Constants & Strong Types
    // ============================================================================

    namespace detail
    {
        static constexpr std::size_t magic_number = 0x7E3CB1A9;
    }

    struct stack_size
    {
        std::size_t value;
        [[nodiscard]] constexpr explicit stack_size(std::size_t v) noexcept : value{v} {}
    };

    struct storage_size
    {
        std::size_t value;
        [[nodiscard]] constexpr explicit storage_size(std::size_t v) noexcept : value{v} {}
    };

    inline constexpr stack_size default_stack_size{UCORO_STACK_SIZE};
    inline constexpr storage_size default_storage_size{UCORO_STORAGE_SIZE};
    inline constexpr stack_size min_stack_size{UCORO_MIN_STACK_SIZE};

    namespace detail
    {
        // ============================================================================
        // Internal Types & Forward Declarations
        // ============================================================================

        extern thread_local struct mco_coro *mco_current_co;

        // Default Allocators (Defined inline to avoid linkage issues)
        inline void *mco_alloc(std::size_t size, void *allocator_data)
        {
            (void)allocator_data;
            return std::calloc(1, size);
        }

        inline void mco_dealloc(void *ptr, std::size_t size, void *allocator_data)
        {
            (void)size;
            (void)allocator_data;
            std::free(ptr);
        }

        enum class mco_state
        {
            dead = 0,
            normal,
            running,
            suspended
        };

        enum class mco_result
        {
            success = 0,
            generic_error,
            invalid_pointer,
            invalid_coroutine,
            not_suspended,
            not_running,
            make_context_error,
            switch_context_error,
            not_enough_space,
            out_of_memory,
            invalid_arguments,
            invalid_operation,
            stack_overflow
        };

        struct mco_coro
        {
            void *context;
            mco_state state;
            void (*func)(mco_coro *co);
            mco_coro *prev_co;
            void *user_data;
            std::size_t coro_size;
            void *allocator_data;
            void (*dealloc_cb)(void *ptr, std::size_t size, void *allocator_data);
            void *stack_base;
            std::size_t stack_size;
            unsigned char *storage;
            std::size_t bytes_stored;
            std::size_t storage_size;
            void *asan_prev_stack;
            void *tsan_prev_fiber;
            void *tsan_fiber;
            std::size_t magic_number;
        };

        struct mco_desc
        {
            void (*func)(mco_coro *co) = nullptr;
            void *user_data = nullptr;
            void *(*alloc_cb)(std::size_t size, void *allocator_data) = nullptr;
            void (*dealloc_cb)(void *ptr, std::size_t size, void *allocator_data) = nullptr;
            void *allocator_data = nullptr;
            std::size_t storage_size = 0;
            std::size_t coro_size = 0;
            std::size_t stack_size = 0;
        };

#if defined(__x86_64__) && !defined(_WIN32)
        struct mco_ctxbuf
        {
            void *rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15;
        };
        struct mco_context
        {
            mco_ctxbuf ctx;
            mco_ctxbuf back_ctx;
        };
        extern "C" int _mco_switch(mco_ctxbuf *from, mco_ctxbuf *to);
#else
#error "Only x86_64 Linux/macOS supported in this version."
#endif

        // ============================================================================
        // Constexpr Helpers (Moved out of IMPL for compile-time usage)
        // ============================================================================

        [[nodiscard]] constexpr std::size_t mco_align_forward(std::size_t addr, std::size_t align)
        {
            return (addr + (align - 1)) & ~(align - 1);
        }

        constexpr void mco_init_desc_sizes(mco_desc *desc, std::size_t stack_size)
        {
            // Calculate exact memory requirements at compile time
            desc->coro_size = mco_align_forward(sizeof(mco_coro), 16) +
                              mco_align_forward(sizeof(mco_context), 16) +
                              mco_align_forward(desc->storage_size, 16) +
                              stack_size + 16;
            desc->stack_size = stack_size;
        }

        [[nodiscard]] constexpr mco_desc mco_desc_init(void (*func)(mco_coro *co), std::size_t stack_size)
        {
            // Zero-init via aggregate initialization
            mco_desc desc{};

            if (stack_size != 0)
            {
                if (stack_size < min_stack_size.value)
                    stack_size = min_stack_size.value;
            }
            else
            {
                stack_size = default_stack_size.value;
            }
            stack_size = mco_align_forward(stack_size, 16);

            desc.alloc_cb = mco_alloc;
            desc.dealloc_cb = mco_dealloc;
            desc.func = func;
            desc.storage_size = default_storage_size.value;

            mco_init_desc_sizes(&desc, stack_size);
            return desc;
        }

        // ============================================================================
        // Runtime Inline Helpers
        // ============================================================================

        inline void mco_prepare_jumpin(mco_coro *co)
        {
            mco_coro *prev_co = mco_current_co;
            co->prev_co = prev_co;
            if (prev_co)
                prev_co->state = mco_state::normal;
            mco_current_co = co;
        }

        inline void mco_prepare_jumpout(mco_coro *co)
        {
            mco_coro *prev_co = co->prev_co;
            co->prev_co = nullptr;
            if (prev_co)
                prev_co->state = mco_state::running;
            mco_current_co = prev_co;
        }

        // Internal API Declarations
        void *mco_get_user_data(mco_coro *co);
        mco_state mco_status(mco_coro *co);
        mco_result mco_resume(mco_coro *co);
        mco_result mco_yield(mco_coro *co);
        mco_result mco_push(mco_coro *co, const void *src, std::size_t len);
        mco_result mco_pop(mco_coro *co, void *dest, std::size_t len);
        mco_result mco_peek(mco_coro *co, void *dest, std::size_t len);
        std::size_t mco_get_bytes_stored(mco_coro *co);
        std::size_t mco_get_storage_size(mco_coro *co);
        mco_coro *mco_running(void);
        mco_result mco_destroy(mco_coro *co);
        mco_result mco_create(mco_coro **out_co, mco_desc *desc);

    } // namespace detail

    // ============================================================================
    // Public API Types
    // ============================================================================

    enum class [[nodiscard]] error : std::uint8_t
    {
        success = static_cast<std::uint8_t>(detail::mco_result::success),
        generic_error = static_cast<std::uint8_t>(detail::mco_result::generic_error),
        invalid_pointer = static_cast<std::uint8_t>(detail::mco_result::invalid_pointer),
        invalid_coroutine = static_cast<std::uint8_t>(detail::mco_result::invalid_coroutine),
        not_suspended = static_cast<std::uint8_t>(detail::mco_result::not_suspended),
        not_running = static_cast<std::uint8_t>(detail::mco_result::not_running),
        make_context_error = static_cast<std::uint8_t>(detail::mco_result::make_context_error),
        switch_context_error = static_cast<std::uint8_t>(detail::mco_result::switch_context_error),
        not_enough_space = static_cast<std::uint8_t>(detail::mco_result::not_enough_space),
        out_of_memory = static_cast<std::uint8_t>(detail::mco_result::out_of_memory),
        invalid_arguments = static_cast<std::uint8_t>(detail::mco_result::invalid_arguments),
        invalid_operation = static_cast<std::uint8_t>(detail::mco_result::invalid_operation),
        stack_overflow = static_cast<std::uint8_t>(detail::mco_result::stack_overflow)
    };

    [[nodiscard]] constexpr auto to_string(error e) noexcept -> std::string_view
    {
        switch (e)
        {
        case error::success:
            return "success";
        case error::generic_error:
            return "generic error";
        case error::invalid_pointer:
            return "invalid pointer";
        case error::invalid_coroutine:
            return "invalid coroutine";
        case error::not_suspended:
            return "coroutine not suspended";
        case error::not_running:
            return "coroutine not running";
        case error::make_context_error:
            return "make context error";
        case error::switch_context_error:
            return "switch context error";
        case error::not_enough_space:
            return "not enough space";
        case error::out_of_memory:
            return "out of memory";
        case error::invalid_arguments:
            return "invalid arguments";
        case error::invalid_operation:
            return "invalid operation";
        case error::stack_overflow:
            return "stack overflow";
        }
        return "unknown error";
    }

    [[nodiscard]] constexpr auto from_impl_result(detail::mco_result r) noexcept -> error
    {
        return static_cast<error>(r);
    }

    enum class [[nodiscard]] state : std::uint8_t
    {
        dead = static_cast<std::uint8_t>(detail::mco_state::dead),
        normal = static_cast<std::uint8_t>(detail::mco_state::normal),
        running = static_cast<std::uint8_t>(detail::mco_state::running),
        suspended = static_cast<std::uint8_t>(detail::mco_state::suspended)
    };

    [[nodiscard]] constexpr auto to_string(state s) noexcept -> std::string_view
    {
        switch (s)
        {
        case state::dead:
            return "dead";
        case state::normal:
            return "normal";
        case state::running:
            return "running";
        case state::suspended:
            return "suspended";
        }
        return "unknown state";
    }

    // ============================================================================
    // Concepts
    // ============================================================================

    template <typename F>
    concept coroutine_callable = std::invocable<F>;

    template <typename T>
    concept storable = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T> && (sizeof(T) <= 1024);

    // ============================================================================
    // Classes
    // ============================================================================

    class coroutine_handle
    {
    public:
        constexpr coroutine_handle() noexcept = default;
        constexpr explicit coroutine_handle(detail::mco_coro *co) noexcept : handle_{co} {}

        [[nodiscard]] auto yield() const noexcept -> std::expected<void, error>
        {
            if (handle_ == nullptr)
                return std::unexpected{error::invalid_coroutine};
            auto const result = from_impl_result(detail::mco_yield(handle_));
            if (result != error::success)
                return std::unexpected{result};
            return {};
        }

        void yield_unchecked() const noexcept
        {
            handle_->state = detail::mco_state::suspended;
            detail::mco_prepare_jumpout(handle_);
            detail::mco_context *context = static_cast<detail::mco_context *>(handle_->context);
            detail::_mco_switch(&context->ctx, &context->back_ctx);
        }

        template <storable T>
        void push_unchecked(T const &value) const noexcept
        {
            std::memcpy(&handle_->storage[handle_->bytes_stored], std::addressof(value), sizeof(T));
            handle_->bytes_stored += sizeof(T);
        }

        template <storable T>
        T pop_unchecked() const noexcept
        {
            T value;
            handle_->bytes_stored -= sizeof(T);
            std::memcpy(&value, &handle_->storage[handle_->bytes_stored], sizeof(T));
            return value;
        }

        [[nodiscard]] auto status() const noexcept -> state { return static_cast<state>(detail::mco_status(handle_)); }
        [[nodiscard]] constexpr auto valid() const noexcept -> bool { return handle_ != nullptr; }
        [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }

        template <typename T>
        [[nodiscard]] auto get_user_data() const noexcept -> T *
        {
            return static_cast<T *>(detail::mco_get_user_data(handle_));
        }

        template <storable T>
        [[nodiscard]] auto push(T const &value) const noexcept -> std::expected<void, error>
        {
            auto const result = from_impl_result(detail::mco_push(handle_, std::addressof(value), sizeof(T)));
            if (result != error::success)
                return std::unexpected{result};
            return {};
        }

        [[nodiscard]] auto push_bytes(std::span<std::byte const> data) const noexcept -> std::expected<void, error>
        {
            auto const result = from_impl_result(detail::mco_push(handle_, data.data(), data.size()));
            if (result != error::success)
                return std::unexpected{result};
            return {};
        }

        template <storable T>
        [[nodiscard]] auto pop() const noexcept -> std::expected<T, error>
        {
            T value{};
            auto const result = from_impl_result(detail::mco_pop(handle_, std::addressof(value), sizeof(T)));
            if (result != error::success)
                return std::unexpected{result};
            return value;
        }

        template <storable T>
        [[nodiscard]] auto peek() const noexcept -> std::expected<T, error>
        {
            T value{};
            auto const result = from_impl_result(detail::mco_peek(handle_, std::addressof(value), sizeof(T)));
            if (result != error::success)
                return std::unexpected{result};
            return value;
        }

        [[nodiscard]] auto bytes_stored() const noexcept -> std::size_t { return detail::mco_get_bytes_stored(handle_); }
        [[nodiscard]] auto storage_capacity() const noexcept -> std::size_t { return detail::mco_get_storage_size(handle_); }
        [[nodiscard]] constexpr auto raw() const noexcept -> detail::mco_coro * { return handle_; }

    private:
        detail::mco_coro *handle_{nullptr};
    };

    [[nodiscard]] inline auto running() noexcept -> coroutine_handle
    {
        return coroutine_handle{detail::mco_running()};
    }

    [[nodiscard]] inline auto yield() noexcept -> std::expected<void, error>
    {
        return running().yield();
    }

    class [[nodiscard]] coroutine
    {
    public:
        using function_type = std::function<void(coroutine_handle)>;

        [[nodiscard]] static auto create(function_type func) noexcept -> std::expected<coroutine, error>
        {
            return create(std::move(func), default_stack_size, default_storage_size);
        }

        [[nodiscard]] static auto create(function_type func, stack_size stack, storage_size storage = default_storage_size) noexcept -> std::expected<coroutine, error>
        {
            if (!func)
                return std::unexpected{error::invalid_arguments};

            auto *wrapper = new (std::nothrow) function_type{std::move(func)};
            if (wrapper == nullptr)
                return std::unexpected{error::out_of_memory};

            detail::mco_desc desc = detail::mco_desc_init(&coroutine::entry_point, stack.value);
            desc.user_data = wrapper;

            if (storage.value != default_storage_size.value)
            {
                desc.storage_size = storage.value;
                auto const align16 = [](std::size_t addr)
                { return (addr + 15UL) & ~15UL; };
                desc.coro_size = align16(sizeof(std::uintptr_t) * 17) + align16(128) + align16(storage.value) + stack.value + 16UL;
            }

            detail::mco_coro *co = nullptr;
            auto const result = from_impl_result(detail::mco_create(&co, &desc));

            if (result != error::success)
            {
                delete wrapper;
                return std::unexpected{result};
            }
            return coroutine{co, wrapper};
        }

        coroutine(coroutine const &) = delete;
        auto operator=(coroutine const &) -> coroutine & = delete;

        coroutine(coroutine &&other) noexcept
            : handle_{std::exchange(other.handle_, nullptr)}, func_wrapper_{std::exchange(other.func_wrapper_, nullptr)} {}

        auto operator=(coroutine &&other) noexcept -> coroutine &
        {
            if (this != &other)
            {
                destroy();
                handle_ = std::exchange(other.handle_, nullptr);
                func_wrapper_ = std::exchange(other.func_wrapper_, nullptr);
            }
            return *this;
        }

        ~coroutine() { destroy(); }

        [[nodiscard]] auto resume() noexcept -> std::expected<void, error>
        {
            if (handle_ == nullptr)
                return std::unexpected{error::invalid_coroutine};
            auto const result = from_impl_result(detail::mco_resume(handle_));
            if (result != error::success)
                return std::unexpected{result};
            return {};
        }

        void resume_unchecked() const noexcept
        {
            handle_->state = detail::mco_state::running;
            detail::mco_prepare_jumpin(handle_);
            detail::mco_context *context = static_cast<detail::mco_context *>(handle_->context);
            detail::_mco_switch(&context->back_ctx, &context->ctx);
        }

        template <storable T>
        void push_unchecked(T const &value) const noexcept { handle().push_unchecked(value); }

        template <storable T>
        T pop_unchecked() const noexcept { return handle().pop_unchecked<T>(); }

        [[nodiscard]] auto status() const noexcept -> state { return static_cast<state>(detail::mco_status(handle_)); }
        [[nodiscard]] auto done() const noexcept -> bool { return status() == state::dead; }
        [[nodiscard]] auto suspended() const noexcept -> bool { return status() == state::suspended; }
        [[nodiscard]] auto is_running() const noexcept -> bool { return status() == state::running; }
        [[nodiscard]] auto valid() const noexcept -> bool { return handle_ != nullptr; }
        [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
        [[nodiscard]] auto handle() const noexcept -> coroutine_handle { return coroutine_handle{handle_}; }
        [[nodiscard]] auto raw() const noexcept -> detail::mco_coro * { return handle_; }

        template <storable T>
        [[nodiscard]] auto push(T const &value) const noexcept -> std::expected<void, error> { return handle().push(value); }

        template <storable T>
        [[nodiscard]] auto pop() const noexcept -> std::expected<T, error> { return handle().pop<T>(); }

        template <storable T>
        [[nodiscard]] auto peek() const noexcept -> std::expected<T, error> { return handle().peek<T>(); }

        [[nodiscard]] auto bytes_stored() const noexcept -> std::size_t { return handle().bytes_stored(); }
        [[nodiscard]] auto storage_capacity() const noexcept -> std::size_t { return handle().storage_capacity(); }

    private:
        explicit coroutine(detail::mco_coro *co, function_type *wrapper) noexcept
            : handle_{co}, func_wrapper_{wrapper} {}

        void destroy() noexcept
        {
            if (handle_ != nullptr)
            {
                detail::mco_destroy(handle_);
                handle_ = nullptr;
            }
            if (func_wrapper_ != nullptr)
            {
                delete func_wrapper_;
                func_wrapper_ = nullptr;
            }
        }

        static void entry_point(detail::mco_coro *co)
        {
            auto *wrapper = static_cast<function_type *>(detail::mco_get_user_data(co));
            if (wrapper != nullptr && *wrapper)
            {
                (*wrapper)(coroutine_handle{co});
            }
        }

        detail::mco_coro *handle_{nullptr};
        function_type *func_wrapper_{nullptr};
    };

    template <storable T>
    class [[nodiscard]] generator
    {
    public:
        class iterator;

        [[nodiscard]] static auto create(std::function<void(coroutine_handle)> func) noexcept -> std::expected<generator, error>
        {
            auto coro_result = coroutine::create(std::move(func));
            if (!coro_result)
                return std::unexpected{coro_result.error()};
            return generator{std::move(*coro_result)};
        }

        [[nodiscard]] auto next() noexcept -> std::expected<std::optional<T>, error>
        {
            if (coro_.done())
                return std::optional<T>{std::nullopt};
            auto resume_result = coro_.resume();
            if (!resume_result)
                return std::unexpected{resume_result.error()};
            if (coro_.done())
                return std::optional<T>{std::nullopt};
            auto value_result = coro_.pop<T>();
            if (!value_result)
                return std::unexpected{value_result.error()};
            return std::optional<T>{std::move(*value_result)};
        }

        [[nodiscard]] auto done() const noexcept -> bool { return coro_.done(); }
        [[nodiscard]] auto begin() -> iterator { return iterator{*this}; }
        [[nodiscard]] static auto end() noexcept -> std::default_sentinel_t { return std::default_sentinel; }

    private:
        explicit generator(coroutine coro) noexcept : coro_{std::move(coro)} {}
        coroutine coro_;
    };

    template <storable T>
    class generator<T>::iterator
    {
    public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;

        explicit iterator(generator &gen) : gen_{&gen} { advance(); }
        [[nodiscard]] auto operator*() const noexcept -> T const & { return *current_; }
        auto operator++() -> iterator &
        {
            advance();
            return *this;
        }
        auto operator++(int) -> iterator
        {
            auto copy = *this;
            advance();
            return copy;
        }
        [[nodiscard]] auto operator==(std::default_sentinel_t) const noexcept -> bool { return !current_.has_value(); }

    private:
        void advance()
        {
            auto result = gen_->next();
            if (result && *result)
                current_ = std::move(**result);
            else
                current_ = std::nullopt;
        }
        generator *gen_;
        std::optional<T> current_;
    };

    template <storable T>
    [[nodiscard]] inline auto yield_value(coroutine_handle h, T const &value) noexcept -> std::expected<void, error>
    {
        auto push_result = h.push(value);
        if (!push_result)
            return push_result;
        return h.yield();
    }

    class task_runner
    {
    public:
        auto add(coroutine &&coro) -> task_runner &
        {
            if (coro.valid() && !coro.done())
                tasks_.push_back(std::move(coro));
            return *this;
        }

        [[nodiscard]] auto run() noexcept -> std::expected<void, error>
        {
            while (!tasks_.empty())
            {
                auto it = tasks_.begin();
                while (it != tasks_.end())
                {
                    if (it->done())
                    {
                        it = tasks_.erase(it);
                        continue;
                    }
                    auto result = it->resume();
                    if (!result && result.error() != error::not_suspended)
                        return result;
                    if (it->done())
                        it = tasks_.erase(it);
                    else
                        ++it;
                }
            }
            return {};
        }

        [[nodiscard]] auto step() noexcept -> std::expected<bool, error>
        {
            auto it = tasks_.begin();
            while (it != tasks_.end())
            {
                if (it->done())
                {
                    it = tasks_.erase(it);
                    continue;
                }
                auto result = it->resume();
                if (!result && result.error() != error::not_suspended)
                    return std::unexpected{result.error()};
                if (it->done())
                    it = tasks_.erase(it);
                else
                    ++it;
            }
            return !tasks_.empty();
        }

        [[nodiscard]] auto size() const noexcept -> std::size_t { return tasks_.size(); }
        [[nodiscard]] auto empty() const noexcept -> bool { return tasks_.empty(); }

    private:
        std::vector<coroutine> tasks_;
    };
} // namespace coro

// ============================================================================
// std::format Support
// ============================================================================

template <>
struct std::formatter<coro::error> : std::formatter<std::string_view>
{
    auto format(coro::error e, std::format_context &ctx) const
    {
        return std::formatter<std::string_view>::format(coro::to_string(e), ctx);
    }
};

template <>
struct std::formatter<coro::state> : std::formatter<std::string_view>
{
    auto format(coro::state s, std::format_context &ctx) const
    {
        return std::formatter<std::string_view>::format(coro::to_string(s), ctx);
    }
};

// ============================================================================
// Internal Implementation (minicoro integrated)
// ============================================================================

#ifdef UCORO_IMPL

// mco_alloc and mco_dealloc definitions REMOVED from here (now inline in header)

namespace coro::detail
{
    thread_local mco_coro *mco_current_co = nullptr;

    extern "C" void _mco_wrap_main(void);

#if defined(__x86_64__) && !defined(_WIN32)
    __asm__(
        ".text\n"
#ifdef __MACH__
        ".globl __mco_wrap_main\n"
        "__mco_wrap_main:\n"
#else
        ".globl _mco_wrap_main\n"
        ".type _mco_wrap_main @function\n"
        ".hidden _mco_wrap_main\n"
        "_mco_wrap_main:\n"
#endif
        "  movq %r13, %rdi\n"
        "  jmpq *%r12\n"
#ifndef __MACH__
        ".size _mco_wrap_main, .-_mco_wrap_main\n"
#endif
    );

    __asm__(
        ".text\n"
#ifdef __MACH__
        ".globl __mco_switch\n"
        "__mco_switch:\n"
#else
        ".globl _mco_switch\n"
        ".type _mco_switch @function\n"
        ".hidden _mco_switch\n"
        "_mco_switch:\n"
#endif
        "  leaq 0x3d(%rip), %rax\n"
        "  movq %rax, (%rdi)\n"
        "  movq %rsp, 8(%rdi)\n"
        "  movq %rbp, 16(%rdi)\n"
        "  movq %rbx, 24(%rdi)\n"
        "  movq %r12, 32(%rdi)\n"
        "  movq %r13, 40(%rdi)\n"
        "  movq %r14, 48(%rdi)\n"
        "  movq %r15, 56(%rdi)\n"
        "  movq 56(%rsi), %r15\n"
        "  movq 48(%rsi), %r14\n"
        "  movq 40(%rsi), %r13\n"
        "  movq 32(%rsi), %r12\n"
        "  movq 24(%rsi), %rbx\n"
        "  movq 16(%rsi), %rbp\n"
        "  movq 8(%rsi), %rsp\n"
        "  jmpq *(%rsi)\n"
        "  ret\n"
#ifndef __MACH__
        ".size _mco_switch, .-_mco_switch\n"
#endif
    );

    static void mco_main(mco_coro *co);

    static mco_result mco_makectx(mco_coro *co, mco_ctxbuf *ctx, void *stack_base, std::size_t stack_size)
    {
        stack_size = stack_size - 128; /* Red Zone */

        std::uintptr_t base_addr = reinterpret_cast<std::uintptr_t>(stack_base);
        std::uintptr_t high_addr = base_addr + stack_size - sizeof(std::size_t);

        void **stack_high_ptr = reinterpret_cast<void **>(high_addr);
        stack_high_ptr[0] = reinterpret_cast<void *>(0xdeaddeaddeaddead);

        ctx->rip = reinterpret_cast<void *>(_mco_wrap_main);
        ctx->rsp = static_cast<void *>(stack_high_ptr);
        ctx->r12 = reinterpret_cast<void *>(mco_main);
        ctx->r13 = static_cast<void *>(co);
        return mco_result::success;
    }
#else
#error "Only x86_64 Linux/macOS supported in this stripped version."
#endif

    static void mco_main(mco_coro *co)
    {
        co->func(co);
        co->state = mco_state::dead;
        mco_context *context = static_cast<mco_context *>(co->context);
        mco_prepare_jumpout(co);
        _mco_switch(&context->ctx, &context->back_ctx);
    }

    static void mco_jumpin(mco_coro *co)
    {
        mco_context *context = static_cast<mco_context *>(co->context);
        mco_prepare_jumpin(co);
        _mco_switch(&context->back_ctx, &context->ctx);
    }

    static void mco_jumpout(mco_coro *co)
    {
        mco_context *context = static_cast<mco_context *>(co->context);
        mco_prepare_jumpout(co);
        _mco_switch(&context->ctx, &context->back_ctx);
    }

    static mco_result mco_create_context(mco_coro *co, mco_desc *desc)
    {
        std::uintptr_t co_addr = reinterpret_cast<std::uintptr_t>(co);
        std::uintptr_t context_addr = mco_align_forward(co_addr + sizeof(mco_coro), 16);
        std::uintptr_t storage_addr = mco_align_forward(context_addr + sizeof(mco_context), 16);
        std::uintptr_t stack_addr = mco_align_forward(storage_addr + desc->storage_size, 16);

        mco_context *context = reinterpret_cast<mco_context *>(context_addr);
        std::memset(context, 0, sizeof(mco_context));

        unsigned char *storage = reinterpret_cast<unsigned char *>(storage_addr);
        void *stack_base = reinterpret_cast<void *>(stack_addr);
        std::size_t stack_size = desc->stack_size;

        mco_result res = mco_makectx(co, &context->ctx, stack_base, stack_size);
        if (res != mco_result::success)
            return res;

        co->context = context;
        co->stack_base = stack_base;
        co->stack_size = stack_size;
        co->storage = storage;
        co->storage_size = desc->storage_size;
        return mco_result::success;
    }

    mco_result mco_init(mco_coro *co, mco_desc *desc)
    {
        if (!co)
            return mco_result::invalid_coroutine;
        if (!desc || !desc->func)
            return mco_result::invalid_arguments;
        if (desc->stack_size < min_stack_size.value)
            return mco_result::invalid_arguments;

        std::memset(co, 0, sizeof(mco_coro));
        mco_result res = mco_create_context(co, desc);
        if (res != mco_result::success)
            return res;

        co->state = mco_state::suspended;
        co->dealloc_cb = desc->dealloc_cb;
        co->coro_size = desc->coro_size;
        co->allocator_data = desc->allocator_data;
        co->func = desc->func;
        co->user_data = desc->user_data;
        co->magic_number = magic_number;
        return mco_result::success;
    }

    mco_result mco_uninit(mco_coro *co)
    {
        if (!co)
            return mco_result::invalid_coroutine;
        if (!(co->state == mco_state::suspended || co->state == mco_state::dead))
            return mco_result::invalid_operation;
        co->state = mco_state::dead;
        return mco_result::success;
    }

    mco_result mco_create(mco_coro **out_co, mco_desc *desc)
    {
        if (!out_co)
            return mco_result::invalid_pointer;
        if (!desc || !desc->alloc_cb || !desc->dealloc_cb)
        {
            *out_co = nullptr;
            return mco_result::invalid_arguments;
        }

        mco_coro *co = static_cast<mco_coro *>(desc->alloc_cb(desc->coro_size, desc->allocator_data));
        if (!co)
        {
            *out_co = nullptr;
            return mco_result::out_of_memory;
        }

        mco_result res = mco_init(co, desc);
        if (res != mco_result::success)
        {
            desc->dealloc_cb(co, desc->coro_size, desc->allocator_data);
            *out_co = nullptr;
            return res;
        }
        *out_co = co;
        return mco_result::success;
    }

    mco_result mco_destroy(mco_coro *co)
    {
        if (!co)
            return mco_result::invalid_coroutine;
        mco_result res = mco_uninit(co);
        if (res != mco_result::success)
            return res;
        if (!co->dealloc_cb)
            return mco_result::invalid_pointer;
        co->dealloc_cb(co, co->coro_size, co->allocator_data);
        return mco_result::success;
    }

    mco_result mco_resume(mco_coro *co)
    {
        if (!co)
            return mco_result::invalid_coroutine;
        if (co->state != mco_state::suspended)
            return mco_result::not_suspended;
        co->state = mco_state::running;
        mco_jumpin(co);
        return mco_result::success;
    }

    mco_result mco_yield(mco_coro *co)
    {
        if (!co)
            return mco_result::invalid_coroutine;
        volatile std::size_t dummy;
        std::uintptr_t stack_addr = reinterpret_cast<std::uintptr_t>(&dummy);
        std::uintptr_t stack_min = reinterpret_cast<std::uintptr_t>(co->stack_base);
        std::uintptr_t stack_max = stack_min + co->stack_size;
        if (co->magic_number != magic_number || stack_addr < stack_min || stack_addr > stack_max)
            return mco_result::stack_overflow;

        if (co->state != mco_state::running)
            return mco_result::not_running;
        co->state = mco_state::suspended;
        mco_jumpout(co);
        return mco_result::success;
    }

    mco_state mco_status(mco_coro *co) { return co ? co->state : mco_state::dead; }
    void *mco_get_user_data(mco_coro *co) { return co ? co->user_data : nullptr; }

    mco_result mco_push(mco_coro *co, const void *src, std::size_t len)
    {
        if (!co)
            return mco_result::invalid_coroutine;
        if (len > 0)
        {
            std::size_t bytes_stored = co->bytes_stored + len;
            if (bytes_stored > co->storage_size)
                return mco_result::not_enough_space;
            if (!src)
                return mco_result::invalid_pointer;
            std::memcpy(&co->storage[co->bytes_stored], src, len);
            co->bytes_stored = bytes_stored;
        }
        return mco_result::success;
    }

    mco_result mco_pop(mco_coro *co, void *dest, std::size_t len)
    {
        if (!co)
            return mco_result::invalid_coroutine;
        if (len > 0)
        {
            if (len > co->bytes_stored)
                return mco_result::not_enough_space;
            std::size_t bytes_stored = co->bytes_stored - len;
            if (dest)
                std::memcpy(dest, &co->storage[bytes_stored], len);
            co->bytes_stored = bytes_stored;
        }
        return mco_result::success;
    }

    mco_result mco_peek(mco_coro *co, void *dest, std::size_t len)
    {
        if (!co)
            return mco_result::invalid_coroutine;
        if (len > 0)
        {
            if (len > co->bytes_stored)
                return mco_result::not_enough_space;
            if (!dest)
                return mco_result::invalid_pointer;
            std::memcpy(dest, &co->storage[co->bytes_stored - len], len);
        }
        return mco_result::success;
    }

    std::size_t mco_get_bytes_stored(mco_coro *co) { return co ? co->bytes_stored : 0; }
    std::size_t mco_get_storage_size(mco_coro *co) { return co ? co->storage_size : 0; }
    mco_coro *mco_running(void) { return mco_current_co; }

} // namespace coro::detail

#endif // UCORO_IMPL