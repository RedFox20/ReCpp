#pragma once
/**
 * Chainable and coroutine compatible futures, which own their shared state
 * Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include "future_types.h" // rpp::wait_result, rpp::coro_handle, rpp::suspend_never
#include "semaphore.h" // rpp::semaphore_once_flag
#include "thread_pool.h" // rpp::parallel_task_detached, which semaphore.h only declares
#include "traits.h" // rpp::task_return_t, rpp::first_arg_type
#include "debugging.h" // __assertion_failure
#include "timepoint.h" // rpp::Duration, rpp::TimePoint
#include <atomic>
#include <exception> // std::exception_ptr, std::terminate
#include <optional>
#include <stdexcept> // std::logic_error
#include <type_traits>
#include <utility> // std::move, std::forward
#include <vector>
// This header must never reach <future>, so a module which carries it stays clear of BUGS.md B28

namespace rpp
{
    template<class T = void>
    class NODISCARD RPP_CORO_RETURN_TYPE RPP_CORO_LIFETIMEBOUND future;

    namespace detail
    {
        /// Stands in for the value of a future<void>, which has none
        struct no_value {};

        /// The result which one promise publishes and one future collects
        template<class T>
        struct future_state
        {
            rpp::semaphore_once_flag done; // the promise sets it once, after it stores the result
            std::atomic_int refs { 1 }; // counts the promise, and the future after get_future()
            std::exception_ptr error;
            std::optional<std::conditional_t<std::is_void_v<T>, no_value, T>> value;

            /// Drops one owner, and deletes the state after the last one
            void release() noexcept
            {
                // acq_rel, so the owner which deletes sees every write of the other owner
                if (refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    delete this;
            }

            /// Moves the value out or rethrows the error, then drops the reference of the future
            T take()
            {
                struct release_on_exit { future_state* s; ~release_on_exit() noexcept { s->release(); } } ref { this };
                if constexpr (std::is_void_v<T>)
                {
                    if (error) std::rethrow_exception(error);
                }
                else
                {
                    if (value) return std::move(*value);
                    std::rethrow_exception(error);
                }
            }
        };

        // task(value) continues a future<T>, and task() continues a future<void>
        template<class T, class Task>
        using continuation = std::conditional_t<std::is_void_v<T>, std::invoke_result<Task&>, std::invoke_result<Task&, T>>;

        /// The decayed result of a continuation. It has no type when `Task` cannot take the result
        template<class T, class Task>
        using next_t = std::decay_t<typename continuation<T, Task>::type>;

        /// Rethrows `e`, because no handler takes it
        template<class R>
        R handle(const std::exception_ptr& e) { std::rethrow_exception(e); }

        /// Passes `e` to the first handler whose argument type matches, as a chain of catch blocks does
        template<class R, class Handler, class... Rest>
        R handle(const std::exception_ptr& e, Handler& handler, Rest&... rest)
        {
            try { std::rethrow_exception(e); }
            catch (first_arg_type<Handler>& ex)
            {
                if constexpr (std::is_void_v<R>) (void)handler(ex);
                else return handler(ex);
            }
            catch (...) { return handle<R>(e, rest...); }
        }
    }


    /**
     * The producer half of an rpp::future, which publishes one value or one exception.
     * A promise whose destructor runs before it publishes gives its future a std::logic_error, so no waiter blocks forever.
     */
    template<class T>
    class promise
    {
    protected:
        detail::future_state<T>* state = new detail::future_state<T>{};
        bool retrieved = false; // get_future() gives exactly one future
        bool published = false;

    public:
        promise() = default;
        promise(promise&& p) noexcept : state{p.state}, retrieved{p.retrieved}, published{p.published} { p.state = nullptr; }

        /// Abandons the state this promise holds, as the destructor does, then takes the state of `p`
        promise& operator=(promise&& p) noexcept
        {
            if (this != &p)
            {
                abandon();
                state = p.state;
                retrieved = p.retrieved;
                published = p.published;
                p.state = nullptr;
            }
            return *this;
        }

        /// Publishes the stored result. A promise with a future and no result publishes a std::logic_error
        ~promise() noexcept { abandon(); }

        /// @returns the future which receives the result. Throws std::logic_error on a second call
        RPP_CORO_WRAPPER future<T> get_future()
        {
            if (!state || retrieved) throw std::logic_error{"rpp::promise has no future to give"};
            retrieved = true;
            state->refs.fetch_add(1, std::memory_order_relaxed); // relaxed, because the promise already owns the state
            return future<T>{state};
        }

        /// Stores the value and wakes the waiter. Throws std::logic_error after the promise published once
        template<class... Args>
        void set_value(Args&&... args)
        {
            live().value.emplace(std::forward<Args>(args)...);
            publish();
        }

        /// Stores the exception which get() rethrows, and wakes the waiter
        void set_exception(std::exception_ptr e)
        {
            live().error = std::move(e);
            publish();
        }

    protected:
        detail::future_state<T>& live() const
        {
            if (!state || published) throw std::logic_error{"rpp::promise already published its result, or it moved"};
            return *state;
        }

        // the promise keeps its reference after the publish, so get_future() still works afterwards
        void publish() noexcept
        {
            published = true;
            state->done.notify_all();
        }

        // the destructor and the move assignment both end the life of a state
        void abandon() noexcept
        {
            if (!state) return;
            if (!published && retrieved && !state->value && !state->error)
            {
                std::logic_error broken { "rpp::promise released its state with no result" };
                state->error = std::make_exception_ptr(broken);
            }
            if (!published) state->done.notify_all(); // the stored result, or the error above
            state->release();
            state = nullptr;
        }
    };


    /**
     * Runs `task` on the rpp::thread_pool.
     * @returns a future which receives the return value of `task`, or the exception it throws
     */
    template<typename Task>
    RPP_CORO_WRAPPER auto async(Task task) noexcept -> future<task_return_t<Task>>
    {
        using T = task_return_t<Task>;
        promise<T> p;
        future<T> f = p.get_future();
        // the pool destroys this lambda late, so reset() runs the task destructor before the result publishes
        rpp::parallel_task_detached([task=std::optional<Task>{std::move(task)}, p=std::move(p)]() mutable noexcept
        {
            try
            {
                if constexpr (std::is_void_v<T>)
                {
                    (*task)();
                    task.reset();
                    p.set_value();
                }
                else
                {
                    T value = (*task)();
                    task.reset();
                    p.set_value(std::move(value));
                }
            }
            catch (...)
            {
                task.reset();
                p.set_exception(std::current_exception());
            }
        });
        return f;
    }


    namespace detail
    {
        /// Coroutine hooks of future<T>. ~promise() publishes after the frame destroys its locals, before its by-value parameters
        template<class T>
        struct coro_promise_base : promise<T>
        {
            RPP_CORO_WRAPPER future<T> get_return_object() { return this->get_future(); }
            rpp::suspend_never initial_suspend() const noexcept { return {}; }
            rpp::suspend_never final_suspend() const noexcept { return {}; }
            void unhandled_exception() noexcept { this->state->error = std::current_exception(); }
        };

        /// Stores the value of `co_return value`
        template<class T>
        struct coro_promise : coro_promise_base<T>
        {
            void return_value(const T& value) { this->state->value.emplace(value); }
            void return_value(T&& value) { this->state->value.emplace(std::move(value)); }
        };

        /// Stores the empty result of `co_return`
        template<>
        struct coro_promise<void> : coro_promise_base<void>
        {
            void return_void() noexcept { this->state->value.emplace(); }
        };
    }


    /**
     * A chainable and awaitable result, which owns its shared state and never names a std future.
     * A ready future collects its result in the destructor. An unready one terminates, because nobody awaited it.
     * @code
     *     rpp::async([=]{
     *         return downloadZipFile(url);
     *     }).then([=](std::string zipPath) {
     *         return extractContents(zipPath);
     *     }).continue_with([=](std::string extractedDir) {
     *         jobComplete(extractedDir);
     *     });
     * @endcode
     */
    template<class T>
    class NODISCARD RPP_CORO_RETURN_TYPE future
    {
        detail::future_state<T>* state = nullptr;

        friend class promise<T>;
        explicit future(detail::future_state<T>* s) noexcept : state{s} {}

    public:
        using value_type = T;
        using promise_type = detail::coro_promise<T>;

        future() noexcept = default;
        future(future&& f) noexcept : state{f.state} { f.state = nullptr; }
        future& operator=(future&& f) noexcept
        {
            if (this != &f)
            {
                drop();
                state = f.state;
                f.state = nullptr;
            }
            return *this;
        }

        /// Collects a ready result. An unready future terminates, because nobody awaited it
        ~future() noexcept { drop(); }

        /// @returns true while this future holds a state, which get() consumes
        bool valid() const noexcept { return state != nullptr; }

        /// @returns true when the result arrived, without a block
        bool await_ready() const noexcept { return state && state->done.is_set(); }

        /// Blocks until the result arrives. Throws std::logic_error on an invalid future
        void wait() const { checked().done.wait(); }

        /// @returns wait_result::finished when the result arrives before the timeout
        wait_result wait_for(rpp::Duration timeout) const
        {
            return checked().done.wait(timeout) == rpp::semaphore::notified ? wait_result::finished : wait_result::timeout;
        }

        /// @returns wait_result::finished when the result arrives before the deadline
        wait_result wait_until(const rpp::TimePoint& until) const { return wait_for(until - rpp::TimePoint::monotonic_now()); }

        /// Blocks for the result and returns it, or rethrows its exception. The future is invalid afterwards
        T get()
        {
            wait();
            detail::future_state<T>* s = state;
            state = nullptr; // invalid from here on, also when take() rethrows
            return s->take();
        }

        /// Collects a finished result without a block. @returns false while the result has not arrived
        bool collect_ready(T* result = nullptr) { return await_ready() && collect_wait(result); }

        /// Blocks for the result, and stores it in `*result` when given. @returns false on an invalid future
        bool collect_wait(T* result = nullptr)
        {
            if (!valid()) return false;
            if constexpr (std::is_void_v<T>) get();
            else if (result) *result = get();
            else (void)get();
            return true;
        }

        /**
         * Continues with `task` on the pool. It takes the result, or no argument after a future<void>.
         * Each handler takes one exception type, and the first handler which matches recovers the chain.
         * @code
         *     rpp::async([=]{
         *         return loadScene(file);
         *     }).then([=](Scene scene) {
         *         return showScene(scene);
         *     }, [=](const scene_load_failed& e) {
         *         return showDefaultScene(); // recover
         *     });
         * @endcode
         * @returns a future which receives the result of `task`, or of the handler which recovered
         */
        template<typename Task, typename... Handlers>
        RPP_CORO_WRAPPER auto then(Task task, Handlers... handlers) noexcept -> future<detail::next_t<T, Task>>
        {
            using R = detail::next_t<T, Task>;
            return rpp::async([f=std::move(*this), task=std::move(task), ...handlers=std::move(handlers)]() mutable -> R
            {
                try { return f.forward_to(task); }
                catch (...) { return detail::handle<R>(std::current_exception(), handlers...); }
            });
        }

        /// Waits for this future, then for `next`. @returns a future which receives the result of `next`
        template<typename U>
        RPP_CORO_WRAPPER auto then(future<U>&& next) noexcept -> future<U>
        {
            return rpp::async([f=std::move(*this), n=std::move(next)]() mutable
            {
                try { (void)f.get(); }
                catch (...) { n.detach(); throw; } // the chain fails with this error, so nobody awaits `next`
                return n.get();
            });
        }

        /**
         * @brief Downcasts this future into a future<void>, which waits for the chain and drops the value
         * @code
         *     co_await rpp::async(operation1).then(operation2).then();
         * @endcode
         */
        RPP_CORO_WRAPPER future<void> then() noexcept
        {
            if constexpr (std::is_void_v<T>) return std::move(*this);
            else return rpp::async([f=std::move(*this)]() mutable { (void)f.get(); });
        }

        /// Runs `task` with the result on the pool, and returns no future. This future is invalid afterwards
        template<typename Task, typename... Handlers>
        void continue_with(Task task, Handlers... handlers) noexcept
        {
            rpp::parallel_task_detached([f=std::move(*this), task=std::move(task), ...handlers=std::move(handlers)]() mutable
            {
                try { (void)f.forward_to(task); }
                catch (...) { detail::handle<void>(std::current_exception(), handlers...); }
            });
        }

        /// Abandons the result, so the destructor does not terminate on it. Nobody sees the exception of the result
        void detach() noexcept
        {
            if (state) state->release();
            state = nullptr;
        }

        /**
         * @brief Runs `task` after this future, so the chain runs in sequence. An invalid future starts `task` at once.
         * @note The chain swallows the exception of every task before the last one
         * @returns this future, which receives the result of `task`
         * @code
         *     rpp::future<void> tasks;
         *     tasks.chain_async([&]{ task1(); }).chain_async([&]{ task2(); });
         *     tasks.get(); // waits for task2, which ran after task1
         * @endcode
         */
        template<typename Task>
        future& chain_async(Task task) noexcept
        {
            if (!valid()) *this = rpp::async(std::move(task));
            else *this = rpp::async([f=std::move(*this), task=std::move(task)]() mutable
            {
                f.wait();
                f.detach(); // a failed task does not stop the chain, so nobody sees its error
                return task();
            });
            return *this;
        }

        /// Chains `next` after this future. An invalid future becomes `next`. @returns this future
        future& chain_async(future&& next) noexcept
        {
            if (valid()) chain_async([n=std::move(next)]() mutable { return n.get(); });
            else *this = std::move(next);
            return *this;
        }

        /// Resumes `cont` on a pool thread after the result arrives. @returns false on an invalid future
        bool await_suspend(rpp::coro_handle<> cont) noexcept
        {
            if (!valid()) return false; // resumes at once, so await_resume throws
            rpp::parallel_task_detached([this, cont]()
            {
                wait();
                cont.resume();
            });
            return true;
        }

        /// @returns the result, or rethrows its exception
        T await_resume() { return get(); }

    private:
        detail::future_state<T>& checked() const
        {
            if (!state) throw std::logic_error{"rpp::future is invalid, so it has no result"};
            return *state;
        }

        // waits for the result, then calls task(value), or task() after a future<void>
        template<typename Task>
        decltype(auto) forward_to(Task& task)
        {
            if constexpr (std::is_void_v<T>)
            {
                get();
                return task();
            }
            else return task(get());
        }

        // the destructor and the move assignment both end the life of a state
        void drop() noexcept
        {
            if (!state) return;
            if (state->done.is_set())
            {
                try { (void)get(); }
                catch (const std::exception& e) { __assertion_failure("rpp::future<T> dropped an exception: %s", e.what()); }
                catch (...) { __assertion_failure("rpp::future<T> dropped an exception of an unknown type"); }
                return; // collected, so only an unready future reaches the terminate below
            }
            // fail fast: std::future blocks here in silence, and that hides the missing await
            __assertion_failure("nobody awaited this rpp::future<T> before it ended");
            std::terminate();
        }
    };


    /// @returns a future which already holds `value`
    template<typename T>
    RPP_CORO_WRAPPER future<T> ready_future(T value)
    {
        promise<T> p;
        future<T> f = p.get_future();
        p.set_value(std::move(value));
        return f;
    }

    /// @returns a future<void> which already finished
    RPP_CORO_WRAPPER inline future<void> ready_future()
    {
        promise<void> p;
        future<void> f = p.get_future();
        p.set_value();
        return f;
    }

    /// @returns a future whose get() rethrows the exception `e` points at
    template<typename T>
    RPP_CORO_WRAPPER future<T> exceptional_future(std::exception_ptr e)
    {
        promise<T> p;
        future<T> f = p.get_future();
        p.set_exception(std::move(e));
        return f;
    }

    /// @returns a future whose get() throws `e`
    template<typename T, typename E>
    RPP_CORO_WRAPPER future<T> exceptional_future(E e)
    {
        return rpp::exceptional_future<T>(std::make_exception_ptr(std::move(e)));
    }

    /// Blocks until every future holds its result, and collects none of them
    template<typename T>
    void wait_all(const std::vector<future<T>>& futures)
    {
        for (const future<T>& f : futures)
            f.wait();
    }

    namespace detail
    {
        /// Collects every future, then rethrows the first exception, so the vector keeps no valid future
        template<class T, class Collect>
        void collect_all(std::vector<future<T>>& futures, const Collect& collect)
        {
            std::exception_ptr first;
            for (future<T>& f : futures)
            {
                try { collect(f); }
                catch (...) { if (!first) first = std::current_exception(); }
            }
            if (first) std::rethrow_exception(first);
        }
    }

    /// @returns every result in order. Collects every future, then rethrows the first exception
    template<typename T>
    std::vector<T> get_all(std::vector<future<T>>& futures)
    {
        std::vector<T> all;
        all.reserve(futures.size());
        detail::collect_all(futures, [&](future<T>& f) { all.emplace_back(f.get()); });
        return all;
    }

    /// Blocks until every future finishes. Collects every future, then rethrows the first exception
    inline void get_all(std::vector<future<void>>& futures)
    {
        detail::collect_all(futures, [](future<void>& f) { f.get(); });
    }
} // namespace rpp
