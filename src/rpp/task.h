#pragma once
/**
 * Eager, single-threaded coroutine task - a structured-concurrency alternative to cfuture<T>
 * that resumes its awaiter on the SAME (event-loop) thread the task completes on, without
 * spawning a thread for the continuation.
 *
 * Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "future_types.h" // RPP_HAS_COROUTINES

#if RPP_HAS_COROUTINES
#include <coroutine>
#include <exception> // std::exception_ptr
#include <utility> // std::exchange, std::move
#include <variant>

namespace rpp
{
    /**
     * @brief Eager, single-threaded coroutine result. Like `rpp::cfuture<T>` it starts running
     *        immediately at the call site; UNLIKE cfuture, awaiting it never spawns a thread —
     *        the awaiter resumes on whatever thread the task body completes on (its event loop).
     *
     * EAGER (start now): the body runs at CONSTRUCTION (`initial_suspend = suspend_never`),
     *   synchronously up to its first background leaf (e.g. `event_loop::run_async` /
     *   `MicrohardTelnet::async_exec`), which dispatches the real work to a worker thread and
     *   suspends. So `radio.do_something()` launches the work *now*, regardless of when (or
     *   whether) you co_await the result — matching the immediacy callers expect from cfuture.
     *   This is "eager", not "deferred": the work is never queued to start "when the loop has time".
     *
     * RESUMES ON THE LOOP (no thread spawn): awaiting registers a continuation; when the task
     *   completes it resumes that continuation directly (symmetric transfer) on the thread the body
     *   finished on. Because the leaves post their resume back to the event loop, a task (and any
     *   task it awaits) completes on the loop thread — so `co_await` lands you back on the loop with
     *   no off-loop hop. This is the whole point versus cfuture, whose awaiter does
     *   `fut.wait(); cont.resume()` on a detached pool thread (off-loop — a silent data race).
     *
     * NOT a future: there is intentionally no `get()` / `wait()`. `task<T>` is for coroutine code
     *   (`co_await`). Use `rpp::cfuture<T>` when you need a thread-blocking future or `.then()`.
     *
     * LIFETIME (eager hazard): because the body runs from construction, a task is in flight before
     *   you await it. It MUST be co_awaited (or otherwise kept alive until it completes) — destroying
     *   a task whose body is still suspended on a background leaf is undefined behaviour, because the
     *   leaf will later resume an already-destroyed frame. The `co_await foo()` pattern is always
     *   safe (the temporary outlives the await). Do not store-and-drop an un-awaited task.
     *
     * @code
     *   rpp::task<bool> set_freq(int f) {
     *       auto r = co_await loop.run_async([&]{ return radio.write(f); }); // dispatched eagerly
     *       co_return r.ok;
     *   }
     *   bool ok = co_await set_freq(1630); // write already in flight; resumes HERE on the loop
     * @endcode
     */
    template <typename T>
    class task;

    namespace detail
    {
        // Final-suspend awaiter shared by every task promise: hands control to the awaiting
        // coroutine via symmetric transfer (no thread hop, no spawn). Works on any promise that
        // exposes a `.continuation` handle.
        struct task_final_awaiter
        {
            bool await_ready() const noexcept { return false; }
            template <class Promise>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) const noexcept
            {
                std::coroutine_handle<> cont = h.promise().continuation;
                return cont ? cont : std::noop_coroutine();
            }
            void await_resume() const noexcept {}
        };

        // Task handle lifecycle + awaiter registration, shared by the value and void specializations.
        // `Promise` must expose a `.continuation` handle (set when this task is awaited).
        template <class Promise>
        class task_base
        {
        public:
            using handle_type = std::coroutine_handle<Promise>;

        protected:
            handle_type handle{};

        public:
            task_base() noexcept = default;
            explicit task_base(handle_type h) noexcept
                : handle{h}
            {
            }
            task_base(task_base&& o) noexcept
                : handle{std::exchange(o.handle, {})}
            {
            }
            task_base& operator=(task_base&& o) noexcept
            {
                if (this != &o)
                {
                    if (handle)
                        handle.destroy();
                    handle = std::exchange(o.handle, {});
                }
                return *this;
            }
            task_base(const task_base&) = delete;
            task_base& operator=(const task_base&) = delete;
            ~task_base()
            {
                if (handle)
                    handle.destroy();
            }

            /// @returns true if the task has finished running (resolved its value or exception).
            ///          Lets non-coroutine drivers (e.g. event_loop::run_until_done) poll for
            ///          completion without awaiting. Not a wait() — it never blocks.
            bool done() const noexcept { return !handle || handle.done(); }

            bool await_ready() const noexcept { return !handle || handle.done(); }
            // EAGER: the body already started at construction (suspend_never), so here we only
            // register the awaiter to be resumed when the task completes. Returns void => the
            // awaiter suspends; the task's final_suspend resumes it (on the loop). If the task
            // already finished, await_ready() short-circuits and this is never called.
            void await_suspend(std::coroutine_handle<> awaiting) noexcept { handle.promise().continuation = awaiting; }
        };

        template <typename T>
        struct task_promise
        {
            std::coroutine_handle<> continuation{}; // awaiter to resume when we finish
            std::variant<std::monostate, T, std::exception_ptr> result{};

            task<T> get_return_object() noexcept;
            std::suspend_never initial_suspend() noexcept { return {}; } // EAGER: start running at construction
            task_final_awaiter final_suspend() noexcept { return {}; }
            void return_value(T value) noexcept { result.template emplace<1>(std::move(value)); }
            void unhandled_exception() noexcept { result.template emplace<2>(std::current_exception()); }
        };

        template <>
        struct task_promise<void>
        {
            std::coroutine_handle<> continuation{};
            std::exception_ptr error{};

            task<void> get_return_object() noexcept;
            std::suspend_never initial_suspend() noexcept { return {}; } // EAGER: start running at construction
            task_final_awaiter final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() noexcept { error = std::current_exception(); }
        };
    } // namespace detail

    template <typename T>
    class [[nodiscard]] task : public detail::task_base<detail::task_promise<T>>
    {
        using base = detail::task_base<detail::task_promise<T>>;

    public:
        using promise_type = detail::task_promise<T>;
        using base::base; // inherit the handle constructor

        T await_resume()
        {
            auto& r = this->handle.promise().result;
            if (std::exception_ptr* ex = std::get_if<2>(&r))
                std::rethrow_exception(*ex);
            return std::move(std::get<1>(r)); // moved out before the frame is destroyed
        }
    };

    /** @brief void specialization, same eager, resume-on-loop lifecycle, no value. */
    template <>
    class [[nodiscard]] task<void> : public detail::task_base<detail::task_promise<void>>
    {
        using base = detail::task_base<detail::task_promise<void>>;

    public:
        using promise_type = detail::task_promise<void>;
        using base::base;

        void await_resume()
        {
            if (this->handle.promise().error)
                std::rethrow_exception(this->handle.promise().error);
        }
    };

    namespace detail
    {
        template <typename T>
        inline task<T> task_promise<T>::get_return_object() noexcept
        {
            return task<T>{std::coroutine_handle<task_promise>::from_promise(*this)};
        }
        inline task<void> task_promise<void>::get_return_object() noexcept
        {
            return task<void>{std::coroutine_handle<task_promise>::from_promise(*this)};
        }
    } // namespace detail
} // namespace rpp
#endif // RPP_HAS_COROUTINES
