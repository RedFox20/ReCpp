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
#include "debugging.h" // __assertion_failure, LogWarning
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

    /// Matches an event loop which runs a posted delegate on its own thread, as rpp::event_loop does
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

            /// Runs the step as a pool task, then each step the depth cap postponed on this thread, and deletes each one
            /// A delegate which binds this method allocates nothing
            RPPAPI void run_pool_task() noexcept;
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

        template<class Step>
        continuation* make_continuation(Step step)
        {
            return new step_continuation<Step>{std::move(step)};
        }

        /// The result which one promise publishes and one future collects
        template<class T>
        struct future_state
        {
            static constexpr int CLAIMED = 1; // one publisher owns the result
            static constexpr int READY = 2; // the result is out
            static constexpr int WAITER = 4; // a thread blocks on `done`

            rpp::semaphore_once_flag done; // wakes a thread which blocks on the result, only after it set WAITER
            std::atomic_int status { 0 };
            std::atomic_int refs { 1 }; // counts the promise, and the future after get_future()
            std::atomic<void*> next { nullptr }; // the step which follows the result, or this state after the result is out
            std::exception_ptr error;
            std::optional<std::conditional_t<std::is_void_v<T>, no_value, T>> value;

            /// @returns true for the one publisher which may store the result
            bool claim() noexcept
            {
                // acq_rel pairs with unclaim(), so the next publisher sees what a failed store left behind
                return (status.fetch_or(CLAIMED, std::memory_order_acq_rel) & CLAIMED) == 0;
            }

            /// Frees the claim after a store which threw, so the result still takes a value or an exception
            void unclaim() noexcept { status.fetch_and(~CLAIMED, std::memory_order_release); }

            /// @returns true after the result arrived, without a block
            bool ready() const noexcept { return (status.load(std::memory_order_acquire) & READY) != 0; }

            /// @returns true when this thread must block on `done`, because the result has not arrived
            bool must_block() noexcept
            {
                // acq_rel pairs with finish(), so either finish() sees WAITER and wakes this thread, or this thread sees READY
                return !ready() && (status.fetch_or(WAITER, std::memory_order_acq_rel) & READY) == 0;
            }

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

            /// Wakes a waiter, then starts the step which follows. A library promise runs it inline on its own pool step
            void finish(bool library_promise) noexcept
            {
                if ((status.fetch_or(READY, std::memory_order_acq_rel) & WAITER) != 0)
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
        /// @param may_run_here lets a result which is already out run `c` inline on this pool step
        template<class T>
        void start_after(future_state<T>* s, continuation* c, bool may_run_here = false) noexcept
        {
            if (!s || !s->attach(c))
                c->start(may_run_here);
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

        /// Passes `e` to the first handler which matches, and logs a warning when no handler takes it
        /// @note LogError() asserts in a debug build, which stops the program
        template<class... Handlers>
        void handle_or_log(const std::exception_ptr& e, Handlers&... handlers) noexcept
        {
            try { handle<void>(e, handlers...); }
            catch (const std::exception& ex) { LogWarning("continue_with() ignores an unhandled exception: %s", ex.what()); }
            catch (...) { LogWarning("continue_with() ignores an unhandled exception of an unknown type"); }
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
        bool library = false; // a promise of the library may run the next step inline on its pool step

    public:
        promise() = default;
        promise(promise&& p) noexcept : state{std::exchange(p.state, nullptr)}, retrieved{p.retrieved}, library{p.library} {}

        /// Abandons the state this promise holds, as the destructor does, then takes the state of `p`
        promise& operator=(promise&& p) noexcept
        {
            if (this != &p)
            {
                abandon();
                state = std::exchange(p.state, nullptr);
                retrieved = p.retrieved;
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

        /// Stores the value and wakes the waiter. Throws std::logic_error when a publisher came first, as std::promise does
        template<class... Args>
        void set_value(Args&&... args)
        {
            detail::future_state<T>& s = claim();
            try { s.value.emplace(std::forward<Args>(args)...); }
            catch (...) { s.unclaim(); throw; } // no value arrived, so set_exception() may still publish the error
            s.finish(library);
        }

        /// Stores the exception which get() rethrows, and wakes the waiter. Throws std::logic_error as set_value() does
        void set_exception(std::exception_ptr e)
        {
            detail::future_state<T>& s = claim();
            s.error = std::exchange(e, nullptr); // libc++ 18 copies an exception_ptr on a move, see BUGS.md C32
            s.finish(library);
        }

    protected:
        // two threads may race to publish, so only the one which claims the state stores the result
        detail::future_state<T>& claim()
        {
            if (!state || !state->claim()) throw std::logic_error{"rpp::promise already published its result, or it moved"};
            return *state;
        }

        // the destructor and the move assignment both end the life of a state
        void abandon() noexcept
        {
            if (!state) return;
            if (state->claim()) // no publisher came, so this publishes the stored result, or the error below
            {
                if (retrieved && !state->value && !state->error)
                {
                    std::logic_error broken { "rpp::promise released its state with no result" };
                    state->error = std::make_exception_ptr(broken);
                }
                state->finish(false); // this runs inside the code which drops the promise, so no step may start inline here
            }
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

        /// Holds the body of a step, built in place from what `make()` returns, until reset() destroys it
        template<class Body>
        struct body_slot
        {
            union { Body body; }; // a union member ends only in reset(), so the step decides when its body ends
            bool alive = true;

            template<class Make> explicit body_slot(const Make& make) : body(make()) {}
            body_slot(const body_slot&) = delete;
            body_slot& operator=(const body_slot&) = delete;
            ~body_slot() noexcept { reset(); }

            /// Destroys the body, once
            void reset() noexcept
            {
                if (alive)
                {
                    alive = false;
                    body.~Body();
                }
            }
        };

        /// The input of a step which follows no result, as the task of async() does
        struct no_input { void detach() noexcept {} };

        /// Runs its body on the result it follows, destroys the body, then publishes what the body returned
        template<class In, class R, class Body>
        struct step_node : continuation
        {
            In input; // the future this step follows
            step_promise<R> output;
            body_slot<Body> slot;

            template<class Make>
            step_node(In in, step_promise<R>&& out, const Make& make)
                : input{std::move(in)}, output{std::move(out)}, slot{make} {}

            // a step which never ran leaves its input unread, so no dropped error fails an assertion
            ~step_node() noexcept override { input.detach(); }

            void run() noexcept override
            {
                std::exception_ptr error; // publishes after the catch ends, so this thread keeps no reference, see BUGS.md C32
                try
                {
                    if constexpr (std::is_void_v<R>)
                    {
                        call();
                        slot.reset(); // the owner of the step frees it late, so the body ends before the publish
                        output.set_value();
                    }
                    else
                    {
                        R result = call();
                        slot.reset();
                        output.set_value(std::move(result));
                    }
                }
                catch (...) { error = std::current_exception(); }
                slot.reset();
                if (error)
                    output.set_exception(std::exchange(error, nullptr));
            }

        private:
            decltype(auto) call()
            {
                if constexpr (std::is_same_v<In, no_input>) return slot.body();
                else return slot.body(input);
            }
        };

        /// A step which runs on the thread of `loop`, outside any pool step
        template<class Loop, class In, class R, class Body>
        struct loop_step_node final : step_node<In, R, Body>
        {
            Loop* loop;

            template<class Make>
            loop_step_node(Loop& l, In in, step_promise<R>&& out, const Make& make)
                : step_node<In, R, Body>{std::move(in), std::move(out), make}, loop{&l} {}

            void start(bool /*may_run_here*/) noexcept override
            {
                // the delegate owns the step, so a loop which drops the delegate still ends the promise of the step
                loop->post([self=std::unique_ptr<continuation>{this}] { run_outside_pool_steps(*self); });
            }
        };

        /// @returns a step after `in`, whose body `make()` builds in place, so the step never moves the body
        template<class In, class R, class Make>
        continuation* make_step(In in, step_promise<R>&& out, const Make& make)
        {
            return new step_node<In, R, std::invoke_result_t<const Make&>>{std::move(in), std::move(out), make};
        }

        /// @returns a step as the other make_step() does, which runs on the thread of `loop`
        template<class In, class R, class Make, class Loop>
        continuation* make_step(In in, step_promise<R>&& out, const Make& make, Loop& loop)
        {
            return new loop_step_node<Loop, In, R, std::invoke_result_t<const Make&>>{loop, std::move(in), std::move(out), make};
        }
    }


    /**
     * Runs `task` on the rpp::thread_pool. The step moves a temporary `task` once.
     * @returns a future which receives the return value of `task`, or the exception it throws
     */
    template<typename Task>
    RPP_CORO_WRAPPER auto async(Task task) noexcept -> future<task_return_t<Task>>
    {
        using R = task_return_t<Task>;
        detail::step_promise<R> p;
        future<R> f = p.get_future();
        auto make = [&] { return std::move(task); };
        detail::make_step(detail::no_input{}, std::move(p), make)->start(false);
        return f;
    }


    namespace detail
    {
        /// Coroutine hooks of future<T>. ~promise() publishes after the locals end, but before the parameters do.
        /// So this is no promise of the library, and the next step starts as a pool task outside the frame
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
        bool await_ready() const noexcept { return state && state->ready(); }

        /// Blocks until the result arrives. Throws std::logic_error on an invalid future
        void wait() const
        {
            detail::future_state<T>& s = checked();
            if (s.must_block()) s.done.wait();
        }

        /// @returns wait_result::finished when the result arrives before the timeout
        wait_result wait_for(rpp::Duration timeout) const
        {
            detail::future_state<T>& s = checked();
            if (!s.must_block()) return wait_result::finished;
            return s.done.wait(timeout) == rpp::semaphore::notified ? wait_result::finished : wait_result::timeout;
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
         * @note The step moves a temporary `task` once, and each temporary handler once
         */
        template<typename Task, typename... Handlers> requires (!IsEventLoop<Task>)
        RPP_CORO_WRAPPER auto then(Task task, Handlers... handlers) noexcept -> future<detail::next_t<T, Task>>
        {
            return chain<detail::next_t<T, Task>>([&] { return recovering(std::move(task), std::move(handlers)...); });
        }

        /// Continues with `task` on the thread of `loop`, as the other then() does on the pool. `loop` must outlive the chain
        template<IsEventLoop Loop, typename Task, typename... Handlers>
        RPP_CORO_WRAPPER auto then(Loop& loop, Task task, Handlers... handlers) noexcept -> future<detail::next_t<T, Task>>
        {
            return chain<detail::next_t<T, Task>>([&] { return recovering(std::move(task), std::move(handlers)...); }, loop);
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
            else return chain<void>([] { return [](future& f) { (void)f.get(); }; });
        }

        /// Runs `task` with the result as then() does, and returns no future. This future is invalid afterwards
        /// @note An error which no handler takes goes to LogWarning(), because a failed task must not stop the program
        template<typename Task, typename... Handlers> requires (!IsEventLoop<Task>)
        void continue_with(Task task, Handlers... handlers) noexcept
        {
            // nobody holds the future of this step, so the step logs an error which no handler takes
            chain<void>([&] { return ignoring_result(std::move(task), std::move(handlers)...); }).detach();
        }

        /// Runs `task` with the result on the thread of `loop`, and returns no future. `loop` must outlive the chain
        template<IsEventLoop Loop, typename Task, typename... Handlers>
        void continue_with(Loop& loop, Task task, Handlers... handlers) noexcept
        {
            chain<void>([&] { return ignoring_result(std::move(task), std::move(handlers)...); }, loop).detach();
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
            else *this = chain<T>([&] { return after_any_result(std::move(task)); });
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
        static auto recovering(Task&& task, Handlers&&... handlers)
        {
            using R = detail::next_t<T, Task>;
            return [task=std::forward<Task>(task), ...handlers=std::forward<Handlers>(handlers)](future& f) mutable -> R
            {
                try { return f.forward_to(task); }
                catch (...) { return detail::handle<R>(std::current_exception(), handlers...); }
            };
        }

        // the step of continue_with(): passes the result to `task`, drops what it returns, and logs an unhandled error
        template<typename Task, typename... Handlers>
        static auto ignoring_result(Task&& task, Handlers&&... handlers)
        {
            return [task=std::forward<Task>(task), ...handlers=std::forward<Handlers>(handlers)](future& f) mutable
            {
                try { (void)f.forward_to(task); }
                catch (...) { detail::handle_or_log(std::current_exception(), handlers...); }
            };
        }

        // the step of chain_async(): runs `task` after this result, whether the result holds a value or an error
        template<typename Task>
        static auto after_any_result(Task&& task)
        {
            return [task=std::forward<Task>(task)](future& f) mutable -> T
            {
                f.detach(); // a failed task does not stop the chain, so nobody sees its error
                return task();
            };
        }

        // runs the body which `make()` builds on this future after its result arrives. @returns a future of what it returns
        // the body owns every value which `make` captures by reference, so the future borrows no argument
        template<class R, class Make, class... Loop>
        RPP_CORO_WRAPPER RPP_CORO_DISABLE_LIFETIMEBOUND future<R> chain(Make make, Loop&... loop) noexcept
        {
            detail::step_promise<R> p;
            future<R> next = p.get_future();
            detail::future_state<T>* s = state;
            detail::start_after(s, detail::make_step(std::move(*this), std::move(p), make, loop...));
            return next;
        }

        // after this result arrives, `next` publishes into the future this returns. `stop_on_error` lets an error end the chain
        template<class U>
        RPP_CORO_WRAPPER future<U> forward_after(future<U>&& next, bool stop_on_error) noexcept
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
                }
                else
                {
                    n.detach(); // the chain fails with this error, so nobody awaits `next`
                    p.set_exception(std::exchange(error, nullptr));
                }
            };
            detail::start_after(s, detail::make_continuation(std::move(step)));
            return result;
        }

        // publishes this result into `p` after it arrives. A result which is already out publishes on this pool step
        void publish_into(detail::step_promise<T>&& p) && noexcept
        {
            detail::future_state<T>* s = state;
            auto make = [] { return [](future& f) -> T { return f.get(); }; };
            detail::start_after(s, detail::make_step(std::move(*this), std::move(p), make), detail::in_pool_step());
        }

        // the destructor and the move assignment both end the life of a state
        void drop() noexcept
        {
            if (!state) return;
            if (state->ready())
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
