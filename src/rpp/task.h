#pragma once
/**
 * Single-threaded coroutine tasks - structured-concurrency alternatives to cfuture<T> that resume
 * their awaiter on the SAME (event-loop) thread the task completes on, without spawning a thread
 * for the continuation. Two flavours share one implementation via a compile-time policy:
 *   - rpp::task<T>      EAGER  - body runs at construction (like cfuture); launches work now.
 *   - rpp::deferred<T>  LAZY   - body runs only when first awaited (or explicitly started).
 *
 * Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "future_types.h" // RPP_HAS_COROUTINES

#if RPP_HAS_COROUTINES
// NOTE: future_types.h already includes <coroutine> and exposes rpp::coro_handle / rpp::suspend_* / RPP_CORO_STD
#include <atomic>
#include <exception> // std::exception_ptr
#include <type_traits> // std::conditional_t, std::is_void_v
#include <utility> // std::exchange, std::move
#include <variant>

namespace rpp
{
    /**
     * @brief Single-threaded coroutine result. Unlike `rpp::cfuture<T>`, awaiting one never spawns
     *        a thread — the awaiter resumes on whatever thread the task body completes on (its loop).
     *
     * Two flavours, identical API, selected at the type level:
     *   - `rpp::task<T>` (EAGER): the body runs at CONSTRUCTION up to its first background leaf
     *     (e.g. `event_loop::run_async`), which dispatches the real work and suspends. The work is
     *     launched immediately at the call site, regardless of when you co_await — matching the
     *     immediacy callers expect from `cfuture`. This is "eager", never "deferred-until-the-loop".
     *   - `rpp::deferred<T>` (LAZY): the body does NOT run until the result is first awaited (or
     *     started via `deferred::start()`). Awaiting starts it via symmetric transfer, so the delay
     *     between `co_await` and launch is a few cycles, not a scheduler round-trip.
     *
     * Both RESUME ON THE LOOP (no thread spawn): awaiting registers a continuation; on completion
     * the task resumes it via symmetric transfer on the thread the body finished on. Because the
     * leaves post their resume back to the event loop, a chain completes on the loop thread — so
     * `co_await` lands you back on the loop with no off-loop hop (the whole point versus cfuture,
     * whose awaiter does `fut.wait(); cont.resume()` on a detached pool thread — off-loop).
     *
     * NOT a future: there is intentionally no `get()` / `wait()`. These are for coroutine code
     * (`co_await`); a top-level task is driven by `event_loop::run_until_done()`. Use
     * `rpp::cfuture<T>` when you need a thread-blocking future or `.then()`.
     *
     * LIFETIME: a running task must outlive its completion — destroying one whose body is still
     * suspended on a background leaf is undefined (the leaf would resume a destroyed frame). The
     * `co_await foo()` pattern is always safe (the temporary outlives the await). An eager `task`
     * is in flight from construction, so never store-and-drop one un-awaited; a `deferred` that was
     * never awaited/started never ran and is safe to drop.
     *
     * @code
     *   rpp::task<bool> set_freq(int f) {                    // EAGER: launches the write now
     *       auto r = co_await loop.run_async([&]{ return radio.write(f); });
     *       co_return r.ok;
     *   }
     *   bool ok = co_await set_freq(1630);                   // write already in flight; resumes on loop
     *
     *   rpp::deferred<int> lazily() { co_return compute(); } // LAZY: runs only when awaited
     * @endcode
     */
    template <typename T>
    class task;
    template <typename T>
    class deferred;

    namespace detail
    {
        // Set by event_loop on the owner thread; captured by task_base::await_suspend so
        // task_final_awaiter posts the continuation back instead of doing an inline transfer.
        using loop_post_fn = void (*)(void* ctx, rpp::coro_handle<>) noexcept;
        struct loop_ctx { loop_post_fn fn = nullptr; void* ctx = nullptr; };
        inline thread_local loop_ctx tl_loop;

        // Sentinel stored by task_final_awaiter to signal "task completed before await_suspend ran".
        inline void* task_done_sentinel() noexcept { return reinterpret_cast<void*>(uintptr_t{1}); }

        struct task_final_awaiter
        {
            bool await_ready() const noexcept { return false; }
            template <class Promise>
            rpp::coro_handle<> await_suspend(rpp::coro_handle<Promise> h) const noexcept
            {
                auto& p = h.promise();
                // release: result is visible to whoever acquires. acquire on failure: we see the
                // resume_via/ctx stores done before await_suspend's release CAS.
                void* expected = nullptr;
                if (!p.continuation.compare_exchange_strong(expected, task_done_sentinel(),
                        std::memory_order_release, std::memory_order_acquire))
                {
                    rpp::coro_handle<> cont = rpp::coro_handle<>::from_address(expected);
                    if (auto fn = p.resume_via.load(std::memory_order_relaxed))
                    {
                        fn(p.resume_ctx.load(std::memory_order_relaxed), cont);
                        return RPP_CORO_STD::noop_coroutine();
                    }
                    return cont;
                }
                return RPP_CORO_STD::noop_coroutine();
            }
            void await_resume() const noexcept {}
        };

        // Extract a completed task's result: rethrows a captured exception, else moves out the value.
        template <typename T, class Handle>
        T take_result(Handle handle)
        {
            if constexpr (std::is_void_v<T>)
            {
                if (handle.promise().error)
                    std::rethrow_exception(handle.promise().error);
            }
            else
            {
                auto& r = handle.promise().result;
                if (std::exception_ptr* ex = std::get_if<2>(&r))
                    std::rethrow_exception(*ex);
                return std::move(std::get<1>(r)); // moved out before the frame is destroyed
            }
        }

        // Promise shared by task (Lazy=false) and deferred (Lazy=true); only initial_suspend differs.
        // `Owner` is the concrete task<T> / deferred<T> that get_return_object must produce.
        template <typename T, bool Lazy, class Owner>
        struct task_promise
        {
            using value_type = T;
            std::atomic<void*> continuation{nullptr};
            std::atomic<loop_post_fn> resume_via{nullptr};
            std::atomic<void*> resume_ctx{nullptr};
            std::variant<std::monostate, T, std::exception_ptr> result{};

            Owner get_return_object() noexcept { return Owner{rpp::coro_handle<task_promise>::from_promise(*this)}; }
            std::conditional_t<Lazy, rpp::suspend_always, rpp::suspend_never> initial_suspend() noexcept { return {}; }
            task_final_awaiter final_suspend() noexcept { return {}; }
            void return_value(T value) noexcept { result.template emplace<1>(std::move(value)); }
            void unhandled_exception() noexcept { result.template emplace<2>(std::current_exception()); }
        };

        template <bool Lazy, class Owner>
        struct task_promise<void, Lazy, Owner>
        {
            using value_type = void;
            std::atomic<void*> continuation{nullptr};
            std::atomic<loop_post_fn> resume_via{nullptr};
            std::atomic<void*> resume_ctx{nullptr};
            std::exception_ptr error{};

            Owner get_return_object() noexcept { return Owner{rpp::coro_handle<task_promise>::from_promise(*this)}; }
            std::conditional_t<Lazy, rpp::suspend_always, rpp::suspend_never> initial_suspend() noexcept { return {}; }
            task_final_awaiter final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() noexcept { error = std::current_exception(); }
        };

        // Handle lifecycle + awaiter, shared by both flavours. `Lazy` selects eager vs deferred start
        // at compile time (zero runtime cost). `Promise::value_type` gives the awaited result type.
        template <class Promise, bool Lazy>
        class task_base
        {
        public:
            using handle_type = rpp::coro_handle<Promise>;

        protected:
            handle_type handle{};
            bool started = false; // deferred-only: guards the one-time initial launch (unused when eager)

        public:
            task_base() noexcept = default;
            explicit task_base(handle_type h) noexcept
                : handle{h}
            {
            }
            task_base(task_base&& o) noexcept
                : handle{std::exchange(o.handle, {})}
                , started{std::exchange(o.started, false)}
            {
            }
            task_base& operator=(task_base&& o) noexcept
            {
                if (this != &o)
                {
                    if (handle)
                        handle.destroy();
                    handle = std::exchange(o.handle, {});
                    started = std::exchange(o.started, false);
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

            /// @returns true once the task has resolved (value or exception). Non-blocking poll;
            ///          lets drivers like event_loop::run_until_done check completion without awaiting.
            bool done() const noexcept { return !handle || handle.done(); }

            // Always false: the CAS in await_suspend must run to coordinate with task_final_awaiter.
            bool await_ready() const noexcept { return false; }

            rpp::coro_handle<> await_suspend(rpp::coro_handle<> awaiting) noexcept
            {
                auto& p = handle.promise();
                // Capture loop context; release CAS below pairs with task_final_awaiter's acquire.
                p.resume_via.store(detail::tl_loop.fn, std::memory_order_relaxed);
                p.resume_ctx.store(detail::tl_loop.ctx, std::memory_order_relaxed);

                if constexpr (Lazy)
                {
                    if (!started)
                    {
                        started = true;
                        p.continuation.store(awaiting.address(), std::memory_order_release);
                        return handle; // symmetric transfer launches the body
                    }
                }

                void* expected = nullptr;
                if (p.continuation.compare_exchange_strong(expected, awaiting.address(),
                        std::memory_order_release, std::memory_order_acquire))
                    return RPP_CORO_STD::noop_coroutine();

                // Task already done — resume using captured loop context.
                if (detail::tl_loop.fn)
                {
                    detail::tl_loop.fn(detail::tl_loop.ctx, awaiting);
                    return RPP_CORO_STD::noop_coroutine();
                }
                return awaiting;
            }

            typename Promise::value_type await_resume() { return detail::take_result<typename Promise::value_type>(handle); }
        };
    } // namespace detail

    /** @brief Eager coroutine task: starts at construction. `co_await` resumes the caller on the loop. */
    template <typename T>
    class [[nodiscard]] task : public detail::task_base<detail::task_promise<T, false, task<T>>, false>
    {
        using base = detail::task_base<detail::task_promise<T, false, task<T>>, false>;

    public:
        using promise_type = detail::task_promise<T, false, task<T>>;
        using base::base;
    };

    /** @brief Lazy coroutine task: starts only when first awaited (or via start()). Same API as task. */
    template <typename T>
    class [[nodiscard]] deferred : public detail::task_base<detail::task_promise<T, true, deferred<T>>, true>
    {
        using base = detail::task_base<detail::task_promise<T, true, deferred<T>>, true>;

    public:
        using promise_type = detail::task_promise<T, true, deferred<T>>;
        using base::base;

        /// @brief Starts a not-yet-started deferred task (resumes it from its initial suspend), so a
        ///        non-coroutine driver can launch it before pumping. `event_loop::run_until_done`
        ///        calls this for you. Idempotent: a second start() — or a co_await after start() —
        ///        does NOT run the body again (a task runs to completion exactly once; re-running
        ///        would make it a generator, a different pattern).
        void start() noexcept
        {
            if (this->handle && !this->started)
            {
                this->started = true;
                this->handle.resume();
            }
        }
    };
} // namespace rpp
#endif // RPP_HAS_COROUTINES
