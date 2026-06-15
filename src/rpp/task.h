#pragma once
/**
 * Lazy, symmetric-transfer coroutine task - a structured-concurrency alternative to cfuture<T>
 * for code that must resume its awaiter on the SAME thread the task completes on.
 *
 * Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "future_types.h" // RPP_HAS_COROUTINES

#if RPP_HAS_COROUTINES
#include <coroutine>
#include <variant>
#include <utility>   // std::exchange, std::move
#include <exception> // std::exception_ptr

namespace rpp
{
    /**
     * @brief Lazy, symmetric-transfer coroutine result. Unlike `rpp::cfuture<T>`, awaiting a
     *        `rpp::task<T>` NEVER spawns a thread and resumes the awaiter on whatever thread the
     *        task body completes on.
     *
     * WHY - the `cfuture<T>` footgun: `co_await someCfuture()` is EAGER and, on every await, spawns
     *   a detached thread-pool task that does `fut.wait(); cont.resume()`, so the continuation runs
     *   inline on a background worker. Inside an event-loop coroutine that silently moves execution
     *   OFF the loop thread, which races loop-owned state. Almost no one expects a bare `co_await`
     *   to launch a thread.
     *
     * HOW `task` differs: it is LAZY (runs only when awaited) and uses SYMMETRIC TRANSFER to start
     *   the task and to hand control back to its awaiter - it never spawns a thread of its own. A
     *   chain of tasks therefore stays on whatever thread drives the leaves; if every leaf await
     *   resumes on an event loop (e.g. `event_loop::delay` / `event_loop::run_async`), the whole
     *   chain stays on that loop end-to-end.
     *
     * NOT a future: there is intentionally no `get()` / `wait()`. `task<T>` is for coroutine code
     *   (`co_await`). Use `rpp::cfuture<T>` when you need a thread-blocking future. A lazy task is
     *   driven either by awaiting it from another coroutine, or - for a top-level task - by an
     *   eager driver such as `rpp::event_task` + `event_loop::run_async_void()`.
     *
     * @code
     *   rpp::task<bool> set_freq(int f) {
     *       auto r = co_await loop.run_async([&]{ return radio.write(f); }); // leaf: resumes on loop
     *       co_return r.ok;
     *   }
     *   bool ok = co_await set_freq(1630); // resumes HERE, on the loop - no background worker
     * @endcode
     */
    template<typename T> class task;

    namespace detail
    {
        // Final-suspend awaiter shared by every task promise: hands control to the awaiting
        // coroutine via symmetric transfer (no thread hop, no spawn). Works on any promise that
        // exposes a `.continuation` handle.
        struct task_final_awaiter
        {
            bool await_ready() const noexcept { return false; }
            template<class Promise>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) const noexcept
            {
                std::coroutine_handle<> cont = h.promise().continuation;
                return cont ? cont : std::noop_coroutine();
            }
            void await_resume() const noexcept {}
        };

        // Lazy-task handle lifecycle + awaiter-start, shared by the value and void specializations.
        // `Promise` must expose a `.continuation` handle (set when this task is awaited).
        template<class Promise>
        class task_base
        {
        public:
            using handle_type = std::coroutine_handle<Promise>;

        protected:
            handle_type handle {};

        public:
            task_base() noexcept = default;
            explicit task_base(handle_type h) noexcept : handle{h} {}
            task_base(task_base&& o) noexcept : handle{std::exchange(o.handle, {})} {}
            task_base& operator=(task_base&& o) noexcept
            {
                if (this != &o) { if (handle) handle.destroy(); handle = std::exchange(o.handle, {}); }
                return *this;
            }
            task_base(const task_base&) = delete;
            task_base& operator=(const task_base&) = delete;
            ~task_base() { if (handle) handle.destroy(); }

            bool await_ready() const noexcept { return !handle || handle.done(); }
            // symmetric transfer: starts THIS lazy task on the awaiting thread
            handle_type await_suspend(std::coroutine_handle<> awaiting) noexcept
            {
                handle.promise().continuation = awaiting;
                return handle;
            }
        };

        template<typename T>
        struct task_promise
        {
            std::coroutine_handle<> continuation {}; // awaiter to resume when we finish
            std::variant<std::monostate, T, std::exception_ptr> result {};

            task<T> get_return_object() noexcept;
            std::suspend_always initial_suspend() noexcept { return {}; } // LAZY: do not run until awaited
            task_final_awaiter final_suspend() noexcept { return {}; }
            void return_value(T value) noexcept { result.template emplace<1>(std::move(value)); }
            void unhandled_exception() noexcept { result.template emplace<2>(std::current_exception()); }
        };

        template<>
        struct task_promise<void>
        {
            std::coroutine_handle<> continuation {};
            std::exception_ptr error {};

            task<void> get_return_object() noexcept;
            std::suspend_always initial_suspend() noexcept { return {}; }
            task_final_awaiter final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() noexcept { error = std::current_exception(); }
        };
    }

    template<typename T>
    class [[nodiscard]] task : public detail::task_base<detail::task_promise<T>>
    {
        using base = detail::task_base<detail::task_promise<T>>;
    public:
        using promise_type = detail::task_promise<T>;
        using base::base; // inherit the handle constructor

        T await_resume()
        {
            auto& r = this->handle.promise().result;
            if (std::exception_ptr* ex = std::get_if<2>(&r)) std::rethrow_exception(*ex);
            return std::move(std::get<1>(r)); // moved out before the frame is destroyed
        }
    };

    /** @brief void specialization, same lazy/symmetric-transfer lifecycle, no value. */
    template<>
    class [[nodiscard]] task<void> : public detail::task_base<detail::task_promise<void>>
    {
        using base = detail::task_base<detail::task_promise<void>>;
    public:
        using promise_type = detail::task_promise<void>;
        using base::base;

        void await_resume()
        {
            if (this->handle.promise().error) std::rethrow_exception(this->handle.promise().error);
        }
    };

    namespace detail
    {
        template<typename T>
        inline task<T> task_promise<T>::get_return_object() noexcept
        {
            return task<T>{std::coroutine_handle<task_promise>::from_promise(*this)};
        }
        inline task<void> task_promise<void>::get_return_object() noexcept
        {
            return task<void>{std::coroutine_handle<task_promise>::from_promise(*this)};
        }
    }
}
#endif // RPP_HAS_COROUTINES
