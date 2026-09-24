#pragma once
/**
 * Chainable and coroutine compatible futures, which own their shared state
 * Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include "future_types.h" // rpp::wait_result, rpp::coro_handle, rpp::suspend_never
#include "semaphore.h" // rpp::semaphore_once_flag
#include "delegate.h" // rpp::delegate, which an event loop runs
#include "traits.h" // rpp::task_return_t, rpp::first_arg_type
#include "debugging.h" // __assertion_failure
#include "timepoint.h" // rpp::Duration, rpp::TimePoint
#include <atomic>
#include <exception> // std::exception_ptr, std::terminate
#include <memory> // std::unique_ptr
#include <optional>
#include <stdexcept> // std::logic_error
#include <type_traits>
#include <utility> // std::move, std::forward, std::exchange
#include <vector>
// This header must never reach <future>, so a module which carries it stays clear of BUGS.md B28

namespace rpp
{
    template<class T = void>
    class NODISCARD RPP_CORO_RETURN_TYPE RPP_CORO_LIFETIMEBOUND future;

    /// Matches a loop which runs a posted callback on its own thread, as rpp::event_loop does
    template<class Loop>
    concept IsEventLoop = requires(Loop& loop, rpp::delegate<void()>&& callback) { loop.post(std::move(callback)); };

    namespace detail
    {
        /// Stands in for the value of a future<void>, which has none
        struct no_value {};

        struct continuation;

        /// Takes ownership of `c`. It runs here when `may_run_here` allows it, else as a new pool task
        RPPAPI void start_step(continuation* c, bool may_run_here) noexcept;

        /// A step which runs once, after the result it follows arrives
        struct continuation
        {
            virtual ~continuation() noexcept = default;

            /// Runs the step on this thread
            virtual void run() noexcept = 0;

            /// Takes ownership of the step, and runs it now or later on the thread which it belongs to
            virtual void start(bool may_run_here) noexcept { start_step(this, may_run_here); }
        };

        /// @returns true while this thread runs a pool step, where a promise of the library may run the next step
        RPPAPI bool in_pool_step() noexcept;

        /// Runs `c` outside any pool step, so the result it publishes starts the next step as a pool task
        RPPAPI void run_outside_pool_steps(continuation& c) noexcept;

        /// Runs `step` on a pool thread
        template<class Step>
        struct step_continuation final : continuation
        {
            Step step;
            explicit step_continuation(Step s) : step{std::move(s)} {}
            void run() noexcept override { step(); }
        };

        /// Runs `step` on the thread of `loop`
        template<class Loop, class Step>
        struct loop_continuation final : continuation
        {
            Loop* loop;
            Step step;
            loop_continuation(Loop& l, Step s) : loop{&l}, step{std::move(s)} {}
            void run() noexcept override { step(); }
            void start(bool /*may_run_here*/) noexcept override
            {
                // the callback owns the step, so a loop which drops the callback still ends the promise of the step
                loop->post([self=std::unique_ptr<continuation>{this}] { run_outside_pool_steps(*self); });
            }
        };

        template<class Step>
        continuation* make_continuation(Step step)
        {
            return new step_continuation<Step>{std::move(step)};
        }

        template<class Step, class Loop>
        continuation* make_continuation(Step step, Loop& loop)
        {
            return new loop_continuation<Loop, Step>{loop, std::move(step)};
        }

        /// The result which one promise publishes and one future collects
        template<class T>
        struct future_state
        {
            rpp::semaphore_once_flag done; // the promise sets it once, after it stores the result
            std::atomic_int refs { 1 }; // counts the promise, and the future after get_future()
            std::atomic<void*> next { nullptr }; // the step which follows the result, or this state after the result is out
            std::exception_ptr error;
            std::optional<std::conditional_t<std::is_void_v<T>, no_value, T>> value;

            /// Drops one owner, and deletes the state after the last one
            void release() noexcept
            {
                // acq_rel, so the owner which deletes sees every write of the other owner
                if (refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    delete this;
            }

            /// Attaches the step which follows. @returns false when the result is already out, so `c` stays with the caller
            bool attach(continuation* c) noexcept
            {
                void* none = nullptr;
                // acq_rel pairs with finish(), so either finish() starts `c`, or this thread sees the result
                return next.compare_exchange_strong(none, c, std::memory_order_acq_rel, std::memory_order_acquire);
            }

            /// Wakes the waiter, then starts the step which follows. A library promise runs it inline on its own pool step
            void finish(bool library_promise) noexcept
            {
                done.notify_all();
                if (void* c = next.exchange(this, std::memory_order_acq_rel))
                    static_cast<continuation*>(c)->start(library_promise && in_pool_step());
            }

            /// Moves the result out and leaves no copy of the error, then drops the reference of the future. See BUGS.md C31
            T take()
            {
                struct release_on_exit { future_state* s; ~release_on_exit() noexcept { s->release(); } } ref { this };
                if constexpr (std::is_void_v<T>)
                {
                    if (error) std::rethrow_exception(std::exchange(error, nullptr));
                }
                else
                {
                    if (value) return std::move(*value);
                    std::rethrow_exception(std::exchange(error, nullptr)); // libc++ 18 copies an exception_ptr on a move
                }
            }
        };

        /// Starts `c` after the result of `s` arrives. An invalid future, or a result which is already out, starts it now
        template<class T>
        void start_after(future_state<T>* s, continuation* c) noexcept
        {
            if (!s || !s->attach(c))
                c->start(false);
        }

        // task(value) continues a future<T>, and task() continues a future<void>
        template<class T, class Task>
        using step_result = std::conditional_t<std::is_void_v<T>, std::invoke_result<Task&>, std::invoke_result<Task&, T>>;

        /// The decayed result of a step. It has no type when `Task` cannot take the result
        template<class T, class Task>
        using next_t = std::decay_t<typename step_result<T, Task>::type>;

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
        bool library = false; // a promise of the library may run the next step inline on its pool step

    public:
        promise() = default;
        promise(promise&& p) noexcept : state{std::exchange(p.state, nullptr)}, retrieved{p.retrieved}, published{p.published},
                                        library{p.library} {}

        /// Abandons the state this promise holds, as the destructor does, then takes the state of `p`
        promise& operator=(promise&& p) noexcept
        {
            if (this != &p)
            {
                abandon();
                state = std::exchange(p.state, nullptr);
                retrieved = p.retrieved;
                published = p.published;
                library = p.library;
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
            live().error = std::exchange(e, nullptr); // libc++ 18 copies an exception_ptr on a move, see BUGS.md C32
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
            state->finish(library);
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
            if (!published) state->finish(library); // the stored result, or the error above
            state->release();
            state = nullptr;
        }
    };


    namespace detail
    {
        /// A promise of the library. It runs the next step inline when it publishes on a pool step
        template<class T>
        struct step_promise : promise<T>
        {
            step_promise() { this->library = true; }
        };

        /// @returns a step which runs `work`, destroys it, then publishes what it returned or threw into `p`
        template<class R, class Work>
        auto publishing_step(Work work, step_promise<R>&& p)
        {
            return [work=std::optional<Work>{std::move(work)}, p=std::move(p)]() mutable noexcept
            {
                std::exception_ptr error; // publishes after the catch ends, so this thread keeps no reference, see BUGS.md C32
                try
                {
                    if constexpr (std::is_void_v<R>)
                    {
                        (*work)();
                        work.reset();
                        p.set_value();
                    }
                    else
                    {
                        R result = (*work)();
                        work.reset();
                        p.set_value(std::move(result));
                    }
                }
                catch (...) { error = std::current_exception(); }
                work.reset();
                if (error)
                    p.set_exception(std::exchange(error, nullptr));
            };
        }
    }


    /**
     * Runs `task` on the rpp::thread_pool.
     * @returns a future which receives the return value of `task`, or the exception it throws
     */
    template<typename Task>
    RPP_CORO_WRAPPER auto async(Task task) noexcept -> future<task_return_t<Task>>
    {
        detail::step_promise<task_return_t<Task>> p;
        future<task_return_t<Task>> f = p.get_future();
        detail::make_continuation(detail::publishing_step(std::move(task), std::move(p)))->start(false);
        return f;
    }


    namespace detail
    {
        /// Coroutine hooks of future<T>. ~promise() publishes after the frame destroys its locals, before its by-value parameters
        template<class T>
        struct coro_promise_base : promise<T>
        {
            coro_promise_base() { this->library = true; } // a frame which ends on a pool step runs the next step there
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
     * The destructor collects a ready result, and a stored exception fails an assertion. An unready one terminates as unawaited.
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
        template<class> friend class future;
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

        /// Collects a ready result, and a stored exception fails an assertion. An unready future terminates as unawaited
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
         * Continues with `task` after the result arrives. It takes the result, or no argument after a future<void>.
         * `task` runs inline on the pool thread which published the result, else as a new pool task.
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
        template<typename Task, typename... Handlers> requires (!IsEventLoop<Task>)
        RPP_CORO_WRAPPER auto then(Task task, Handlers... handlers) noexcept -> future<detail::next_t<T, Task>>
        {
            return chain<detail::next_t<T, Task>>(recovering(std::move(task), std::move(handlers)...));
        }

        /// Continues with `task` on the thread of `loop`, as the other then() does on the pool. `loop` must outlive the chain
        template<IsEventLoop Loop, typename Task, typename... Handlers>
        RPP_CORO_WRAPPER auto then(Loop& loop, Task task, Handlers... handlers) noexcept -> future<detail::next_t<T, Task>>
        {
            return chain<detail::next_t<T, Task>>(recovering(std::move(task), std::move(handlers)...), loop);
        }

        /// Follows this future with `next`, and parks no thread. @returns a future which receives the result of `next`
        template<typename U>
        RPP_CORO_WRAPPER auto then(future<U>&& next) noexcept -> future<U>
        {
            return forward_after(std::move(next), true);
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
            else return chain<void>([](future& f) { (void)f.get(); });
        }

        /// Runs `task` with the result as then() does, and returns no future. This future is invalid afterwards
        template<typename Task, typename... Handlers> requires (!IsEventLoop<Task>)
        void continue_with(Task task, Handlers... handlers) noexcept
        {
            // nobody holds the future of this step, so it drops an error which no handler takes
            chain<void>(ignoring_result(std::move(task), std::move(handlers)...)).detach();
        }

        /// Runs `task` with the result on the thread of `loop`, and returns no future. `loop` must outlive the chain
        template<IsEventLoop Loop, typename Task, typename... Handlers>
        void continue_with(Loop& loop, Task task, Handlers... handlers) noexcept
        {
            chain<void>(ignoring_result(std::move(task), std::move(handlers)...), loop).detach();
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
            else *this = chain<T>([task=std::move(task)](future& f) mutable -> T
            {
                f.detach(); // a failed task does not stop the chain, so nobody sees its error
                return task();
            });
            return *this;
        }

        /// Chains `next` after this future. An invalid future becomes `next`. @returns this future
        future& chain_async(future&& next) noexcept
        {
            if (valid()) *this = forward_after(std::move(next), false);
            else *this = std::move(next);
            return *this;
        }

        /// Resumes `cont` after the result arrives, as then() runs a task. @returns false when `cont` goes on at once
        bool await_suspend(rpp::coro_handle<> cont) noexcept
        {
            if (!valid()) return false; // resumes at once, so await_resume throws
            detail::continuation* resume = detail::make_continuation([cont] { cont.resume(); });
            if (state->attach(resume)) return true; // the publisher resumes `cont`, maybe before this line returns
            delete resume;
            return false; // the result arrived meanwhile
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

        // the step of then(): passes the result to `task`, or the error to the first handler which matches
        template<typename Task, typename... Handlers>
        static auto recovering(Task task, Handlers... handlers)
        {
            using R = detail::next_t<T, Task>;
            return [task=std::move(task), ...handlers=std::move(handlers)](future& f) mutable -> R
            {
                try { return f.forward_to(task); }
                catch (...) { return detail::handle<R>(std::current_exception(), handlers...); }
            };
        }

        // the step of continue_with(): passes the result to `task`, and drops what it returns
        template<typename Task, typename... Handlers>
        static auto ignoring_result(Task task, Handlers... handlers)
        {
            return [task=std::move(task), ...handlers=std::move(handlers)](future& f) mutable
            {
                try { (void)f.forward_to(task); }
                catch (...) { detail::handle<void>(std::current_exception(), handlers...); }
            };
        }

        // runs `body(f)` with this future `f` after its result arrives. @returns a future which receives what `body` returns
        template<class R, class Body, class... Loop>
        future<R> chain(Body body, Loop&... loop) noexcept
        {
            detail::step_promise<R> p;
            future<R> next = p.get_future();
            detail::future_state<T>* s = state;
            auto work = [f=std::move(*this), body=std::move(body)]() mutable -> R { return body(f); };
            detail::start_after(s, detail::make_continuation(detail::publishing_step(std::move(work), std::move(p)), loop...));
            return next;
        }

        // after this result arrives, `next` publishes into the future this returns. `stop_on_error` lets an error end the chain
        template<class U>
        future<U> forward_after(future<U>&& next, bool stop_on_error) noexcept
        {
            detail::step_promise<U> p;
            future<U> result = p.get_future();
            detail::future_state<T>* s = state;
            auto step = [f=std::move(*this), n=std::move(next), p=std::move(p), stop_on_error]() mutable noexcept
            {
                std::exception_ptr error; // publishes after the catch ends, see BUGS.md C32
                try { (void)f.get(); }
                catch (...) { if (stop_on_error) error = std::current_exception(); }
                if (!error)
                {
                    std::move(n).publish_into(std::move(p));
                    return;
                }
                n.detach(); // the chain fails with this error, so nobody awaits `next`
                p.set_exception(std::exchange(error, nullptr));
            };
            detail::start_after(s, detail::make_continuation(std::move(step)));
            return result;
        }

        // publishes the result of this future into `p` after it arrives
        void publish_into(detail::step_promise<T>&& p) && noexcept
        {
            detail::future_state<T>* s = state;
            auto work = [f=std::move(*this)]() mutable -> T { return f.get(); };
            detail::start_after(s, detail::make_continuation(detail::publishing_step(std::move(work), std::move(p))));
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
        p.set_exception(std::exchange(e, nullptr)); // the caller may destroy `e` late, see BUGS.md C32
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
