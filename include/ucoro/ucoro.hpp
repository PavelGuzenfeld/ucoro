// ucoro.hpp - C++23 wrapper for ucoro
// because someone thought 3000 lines of preprocessor macros was a good idea
//
// SPDX-License-Identifier: MIT OR Unlicense
// Original C library: Eduardo Bart (https://github.com/edubart/ucoro)
// C++23 wrapper: written with tears and caffeine

#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
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

// we include the C header but wrap it in a namespace so it doesn't
// pollute our beautiful modern code
extern "C"
{
#include "impl/minicoro_impl.h"
}

namespace coro
{

    // ============================================================================
    // error handling - because C error codes are for people who hate themselves
    // ============================================================================

    enum class [[nodiscard]] error : std::uint8_t
    {
        success = MCO_SUCCESS,
        generic_error = MCO_GENERIC_ERROR,
        invalid_pointer = MCO_INVALID_POINTER,
        invalid_coroutine = MCO_INVALID_COROUTINE,
        not_suspended = MCO_NOT_SUSPENDED,
        not_running = MCO_NOT_RUNNING,
        make_context_error = MCO_MAKE_CONTEXT_ERROR,
        switch_context_error = MCO_SWITCH_CONTEXT_ERROR,
        not_enough_space = MCO_NOT_ENOUGH_SPACE,
        out_of_memory = MCO_OUT_OF_MEMORY,
        invalid_arguments = MCO_INVALID_ARGUMENTS,
        invalid_operation = MCO_INVALID_OPERATION,
        stack_overflow = MCO_STACK_OVERFLOW
    };

    // comparison operators
    [[nodiscard]] constexpr auto operator==(error lhs, error rhs) noexcept -> bool
    {
        return static_cast<std::uint8_t>(lhs) == static_cast<std::uint8_t>(rhs);
    }

    [[nodiscard]] constexpr auto operator!=(error lhs, error rhs) noexcept -> bool
    {
        return !(lhs == rhs);
    }

    [[nodiscard]] constexpr auto to_string(error e) noexcept -> std::string_view
    {
        // using switch with returns, no default (cpp23 best practices ch. 55)
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
        return "unknown error"; // unreachable but silences warnings
    }

    // convert from the C error code to our beautiful enum
    [[nodiscard]] constexpr auto from_c_result(mco_result r) noexcept -> error
    {
        return static_cast<error>(r);
    }

    // ============================================================================
    // state enum - scoped because unscoped enums are for C programmers
    // ============================================================================

    enum class [[nodiscard]] state : std::uint8_t
    {
        dead = MCO_DEAD,
        normal = MCO_NORMAL,
        running = MCO_RUNNING,
        suspended = MCO_SUSPENDED
    };

    [[nodiscard]] constexpr auto operator==(state lhs, state rhs) noexcept -> bool
    {
        return static_cast<std::uint8_t>(lhs) == static_cast<std::uint8_t>(rhs);
    }

    [[nodiscard]] constexpr auto operator!=(state lhs, state rhs) noexcept -> bool
    {
        return !(lhs == rhs);
    }

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
    // concepts - constraining template parameters (cpp23 best practices ch. 35)
    // ============================================================================

    // concept for callable coroutine functions
    template <typename F>
    concept coroutine_callable = std::invocable<F>;

    // concept for types that can be pushed/popped from storage
    template <typename T>
    concept storable = std::is_trivially_copyable_v<T> &&
                       std::is_standard_layout_v<T> &&
                       (sizeof(T) <= 1024); // storage size limit

    // ============================================================================
    // configuration - stronger types (cpp23 best practices ch. 28)
    // ============================================================================

    // we use strong types instead of raw size_t because mixing up stack_size
    // and storage_size is exactly the kind of bug you'd write at 3am
    struct stack_size
    {
        std::size_t value;

        [[nodiscard]] constexpr explicit stack_size(std::size_t v) noexcept
            : value{v} {}
    };

    struct storage_size
    {
        std::size_t value;

        [[nodiscard]] constexpr explicit storage_size(std::size_t v) noexcept
            : value{v} {}
    };

    // default values as inline constexpr (cpp23 best practices ch. 44)
    inline constexpr stack_size default_stack_size{56UL * 1024UL};
    inline constexpr storage_size default_storage_size{1024UL};
    inline constexpr stack_size min_stack_size{32768UL};

    // ============================================================================
    // forward declarations
    // ============================================================================

    class coroutine;
    class coroutine_handle;

    // ============================================================================
    // coroutine_handle - non-owning view of a coroutine (rule of 0)
    // ============================================================================

    class [[nodiscard]] coroutine_handle
    {
    public:
        constexpr coroutine_handle() noexcept = default;
        constexpr explicit coroutine_handle(mco_coro *co) noexcept : handle_{co} {}

        // rule of 0 - no custom destructor needed, we don't own the coroutine
        coroutine_handle(coroutine_handle const &) noexcept = default;
        coroutine_handle(coroutine_handle &&) noexcept = default;
        auto operator=(coroutine_handle const &) noexcept -> coroutine_handle & = default;
        auto operator=(coroutine_handle &&) noexcept -> coroutine_handle & = default;
        ~coroutine_handle() = default;

        // yield the current coroutine
        [[nodiscard]] auto yield() const noexcept -> std::expected<void, error>
        {
            if (handle_ == nullptr)
            {
                return std::unexpected{error::invalid_coroutine};
            }
            auto const result = from_c_result(mco_yield(handle_));
            if (result != error::success)
            {
                return std::unexpected{result};
            }
            return {};
        }

        // get current state
        [[nodiscard]] auto status() const noexcept -> state
        {
            return static_cast<state>(mco_status(handle_));
        }

        // check if coroutine is valid
        [[nodiscard]] constexpr auto valid() const noexcept -> bool
        {
            return handle_ != nullptr;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return valid();
        }

        // get user data - returns optional because null is a valid "no data" state
        template <typename T>
        [[nodiscard]] auto get_user_data() const noexcept -> T *
        {
            return static_cast<T *>(mco_get_user_data(handle_));
        }

        // push data to storage
        template <storable T>
        [[nodiscard]] auto push(T const &value) const noexcept -> std::expected<void, error>
        {
            auto const result = from_c_result(
                mco_push(handle_, std::addressof(value), sizeof(T)));
            if (result != error::success)
            {
                return std::unexpected{result};
            }
            return {};
        }

        // push raw bytes
        [[nodiscard]] auto push_bytes(std::span<std::byte const> data) const noexcept
            -> std::expected<void, error>
        {
            auto const result = from_c_result(
                mco_push(handle_, data.data(), data.size()));
            if (result != error::success)
            {
                return std::unexpected{result};
            }
            return {};
        }

        // pop data from storage
        template <storable T>
        [[nodiscard]] auto pop() const noexcept -> std::expected<T, error>
        {
            T value{};
            auto const result = from_c_result(
                mco_pop(handle_, std::addressof(value), sizeof(T)));
            if (result != error::success)
            {
                return std::unexpected{result};
            }
            return value;
        }

        // peek data without consuming
        template <storable T>
        [[nodiscard]] auto peek() const noexcept -> std::expected<T, error>
        {
            T value{};
            auto const result = from_c_result(
                mco_peek(handle_, std::addressof(value), sizeof(T)));
            if (result != error::success)
            {
                return std::unexpected{result};
            }
            return value;
        }

        // get bytes stored
        [[nodiscard]] auto bytes_stored() const noexcept -> std::size_t
        {
            return mco_get_bytes_stored(handle_);
        }

        // get storage size
        [[nodiscard]] auto storage_capacity() const noexcept -> std::size_t
        {
            return mco_get_storage_size(handle_);
        }

        // get raw handle (escape hatch for C interop)
        [[nodiscard]] constexpr auto raw() const noexcept -> mco_coro *
        {
            return handle_;
        }

    private:
        mco_coro *handle_{nullptr};
    };

    // get the currently running coroutine
    [[nodiscard]] inline auto running() noexcept -> coroutine_handle
    {
        return coroutine_handle{mco_running()};
    }

    // yield from anywhere (convenience function)
    [[nodiscard]] inline auto yield() noexcept -> std::expected<void, error>
    {
        return running().yield();
    }

    // ============================================================================
    // coroutine - owning RAII wrapper (rule of 5 because we manage resources)
    // ============================================================================

    class [[nodiscard]] coroutine
    {
    public:
        // function type that the coroutine will execute
        using function_type = std::function<void(coroutine_handle)>;

        // construct with defaults
        [[nodiscard]] static auto create(function_type func) noexcept
            -> std::expected<coroutine, error>
        {
            return create(std::move(func), default_stack_size, default_storage_size);
        }

        // construct with custom stack size
        [[nodiscard]] static auto create(
            function_type func,
            stack_size stack,
            storage_size storage = default_storage_size) noexcept -> std::expected<coroutine, error>
        {

            if (!func)
            {
                return std::unexpected{error::invalid_arguments};
            }

            // allocate wrapper for the function
            auto *wrapper = new (std::nothrow) function_type{std::move(func)};
            if (wrapper == nullptr)
            {
                return std::unexpected{error::out_of_memory};
            }

            // initialize descriptor
            mco_desc desc = mco_desc_init(
                &coroutine::entry_point,
                stack.value);
            desc.user_data = wrapper;

            // if using non-default storage size, we need to recalculate coro_size
            if (storage.value != MCO_DEFAULT_STORAGE_SIZE)
            {
                desc.storage_size = storage.value;
                // recalculate coro_size: the formula is from mco_init_desc_sizes
                // align each component to 16 bytes, plus stack, plus 16 bytes padding
                auto const align16 = [](std::size_t addr) -> std::size_t
                {
                    return (addr + 15UL) & ~15UL;
                };
                // we don't have access to sizeof(mco_context) but it's documented at ~128 bytes
                // for x86_64 it's 2 * sizeof(mco_ctxbuf) = 2 * 64 = 128 bytes
                constexpr std::size_t mco_context_size = 128UL;
                desc.coro_size = align16(sizeof(mco_coro)) +
                                 align16(mco_context_size) +
                                 align16(storage.value) +
                                 stack.value + 16UL;
            }

            // create the coroutine
            mco_coro *co = nullptr;
            auto const result = from_c_result(mco_create(&co, &desc));

            if (result != error::success)
            {
                delete wrapper;
                return std::unexpected{result};
            }

            return coroutine{co, wrapper};
        }

        // rule of 5 - we manage the coroutine lifetime
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

        ~coroutine()
        {
            destroy();
        }

        // resume the coroutine
        [[nodiscard]] auto resume() noexcept -> std::expected<void, error>
        {
            if (handle_ == nullptr)
            {
                return std::unexpected{error::invalid_coroutine};
            }
            auto const result = from_c_result(mco_resume(handle_));
            if (result != error::success)
            {
                return std::unexpected{result};
            }
            return {};
        }

        // get current state
        [[nodiscard]] auto status() const noexcept -> state
        {
            return static_cast<state>(mco_status(handle_));
        }

        // check if done
        [[nodiscard]] auto done() const noexcept -> bool
        {
            return status() == state::dead;
        }

        // check if suspended (can be resumed)
        [[nodiscard]] auto suspended() const noexcept -> bool
        {
            return status() == state::suspended;
        }

        // check if running
        [[nodiscard]] auto is_running() const noexcept -> bool
        {
            return status() == state::running;
        }

        // check if valid
        [[nodiscard]] auto valid() const noexcept -> bool
        {
            return handle_ != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return valid();
        }

        // get non-owning handle
        [[nodiscard]] auto handle() const noexcept -> coroutine_handle
        {
            return coroutine_handle{handle_};
        }

        // get raw handle for C interop
        [[nodiscard]] auto raw() const noexcept -> mco_coro *
        {
            return handle_;
        }

        // push data to storage
        template <storable T>
        [[nodiscard]] auto push(T const &value) const noexcept -> std::expected<void, error>
        {
            return handle().push(value);
        }

        // pop data from storage
        template <storable T>
        [[nodiscard]] auto pop() const noexcept -> std::expected<T, error>
        {
            return handle().pop<T>();
        }

        // peek data without consuming
        template <storable T>
        [[nodiscard]] auto peek() const noexcept -> std::expected<T, error>
        {
            return handle().peek<T>();
        }

        // bytes stored
        [[nodiscard]] auto bytes_stored() const noexcept -> std::size_t
        {
            return handle().bytes_stored();
        }

        // storage capacity
        [[nodiscard]] auto storage_capacity() const noexcept -> std::size_t
        {
            return handle().storage_capacity();
        }

    private:
        explicit coroutine(mco_coro *co, function_type *wrapper) noexcept
            : handle_{co}, func_wrapper_{wrapper} {}

        void destroy() noexcept
        {
            if (handle_ != nullptr)
            {
                // the C library handles destruction gracefully
                mco_destroy(handle_);
                handle_ = nullptr;
            }
            if (func_wrapper_ != nullptr)
            {
                delete func_wrapper_;
                func_wrapper_ = nullptr;
            }
        }

        // C entry point that calls our std::function
        static void entry_point(mco_coro *co)
        {
            auto *wrapper = static_cast<function_type *>(mco_get_user_data(co));
            if (wrapper != nullptr && *wrapper)
            {
                (*wrapper)(coroutine_handle{co});
            }
        }

        mco_coro *handle_{nullptr};
        function_type *func_wrapper_{nullptr};
    };

    // ============================================================================
    // generator - a coroutine that yields values (like python generators)
    // ============================================================================

    template <storable T>
    class [[nodiscard]] generator
    {
    public:
        class iterator;

        [[nodiscard]] static auto create(std::function<void(coroutine_handle)> func) noexcept
            -> std::expected<generator, error>
        {
            auto coro_result = coroutine::create(std::move(func));
            if (!coro_result)
            {
                return std::unexpected{coro_result.error()};
            }
            return generator{std::move(*coro_result)};
        }

        // get next value
        [[nodiscard]] auto next() noexcept -> std::expected<std::optional<T>, error>
        {
            if (coro_.done())
            {
                return std::optional<T>{std::nullopt};
            }

            auto resume_result = coro_.resume();
            if (!resume_result)
            {
                return std::unexpected{resume_result.error()};
            }

            if (coro_.done())
            {
                return std::optional<T>{std::nullopt};
            }

            auto value_result = coro_.pop<T>();
            if (!value_result)
            {
                return std::unexpected{value_result.error()};
            }

            return std::optional<T>{std::move(*value_result)};
        }

        // check if exhausted
        [[nodiscard]] auto done() const noexcept -> bool
        {
            return coro_.done();
        }

        // range support (basic, non-standard but useful)
        [[nodiscard]] auto begin() -> iterator
        {
            return iterator{*this};
        }

        [[nodiscard]] static auto end() noexcept -> std::default_sentinel_t
        {
            return std::default_sentinel;
        }

    private:
        explicit generator(coroutine coro) noexcept : coro_{std::move(coro)} {}

        coroutine coro_;
    };

    // simple iterator for generator - not fully standard compliant but functional
    template <storable T>
    class generator<T>::iterator
    {
    public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;

        explicit iterator(generator &gen) : gen_{&gen}
        {
            advance();
        }

        [[nodiscard]] auto operator*() const noexcept -> T const &
        {
            return *current_;
        }

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

        [[nodiscard]] auto operator==(std::default_sentinel_t) const noexcept -> bool
        {
            return !current_.has_value();
        }

    private:
        void advance()
        {
            auto result = gen_->next();
            if (result && *result)
            {
                current_ = std::move(**result);
            }
            else
            {
                current_ = std::nullopt;
            }
        }

        generator *gen_;
        std::optional<T> current_;
    };

    // ============================================================================
    // utility functions for yielding values in generators
    // ============================================================================

    template <storable T>
    [[nodiscard]] inline auto yield_value(coroutine_handle h, T const &value) noexcept
        -> std::expected<void, error>
    {
        auto push_result = h.push(value);
        if (!push_result)
        {
            return push_result;
        }
        return h.yield();
    }

    // ============================================================================
    // simple async task runner (cooperative multitasking)
    // ============================================================================

    class task_runner
    {
    public:
        // add a coroutine to the run queue
        auto add(coroutine &&coro) -> task_runner &
        {
            if (coro.valid() && !coro.done())
            {
                tasks_.push_back(std::move(coro));
            }
            return *this;
        }

        // run all tasks until completion (round-robin scheduling)
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
                    {
                        return result;
                    }

                    if (it->done())
                    {
                        it = tasks_.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
            return {};
        }

        // run a single step (resume each task once)
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
                {
                    return std::unexpected{result.error()};
                }

                if (it->done())
                {
                    it = tasks_.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            // return true if there are still tasks remaining
            return !tasks_.empty();
        }

        // number of active tasks
        [[nodiscard]] auto size() const noexcept -> std::size_t
        {
            return tasks_.size();
        }

        // check if empty
        [[nodiscard]] auto empty() const noexcept -> bool
        {
            return tasks_.empty();
        }

    private:
        std::vector<coroutine> tasks_;
    };

} // namespace coro

// ============================================================================
// std::format support (cpp23 best practices ch. 42)
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
