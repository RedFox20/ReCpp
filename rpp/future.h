#pragma once
/**
 * Minimal resumable future extensions for C++11 std::future, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 * @note Original implementation description by Herb Sutter at C++ and beyond 2012
 */
#include <memory> // shared_ptr
#include <future>
#include "thread_pool.h"
#include <type_traits>

namespace rpp
{
    template<class T> class NODISCARD cfuture;

    ////////////////////////////////////////////////////////////////////////////////


    template<class T> struct cpromise : std::promise<T>
    {
        template<class Task> void compose(Task& task)
        {
            T value = task();
            // run task destructor before calling continuation
            // provides deterministic sequencing: DownloadAndSaveFile().then(OpenAndParseFile);
            if (!std::is_trivially_destructible_v<Task>) {
                Task t = std::move(task);
                (void)t;
            }
            std::promise<T>::set_value(std::move(value));
        }
    };

    template<> struct cpromise<void> : std::promise<void>
    {
        template<class Task> void compose(Task& task)
        {
            task();
            // run task destructor before calling continuation
            // provides deterministic sequencing: DownloadAndSaveFile().then(OpenAndParseFile);
            if (!std::is_trivially_destructible_v<Task>) {
                Task t = std::move(task);
                (void)t;
            }
            std::promise<void>::set_value();
        }
    };


    template<class Task>
    auto async_task(Task&& task) -> cfuture<decltype(task())>
    {
        using T = decltype(task());
        cpromise<T> p;
        std::shared_future<T> f = p.get_future().share();
        rpp::parallel_task([move_args(p, task)]() mutable
        {
            try {
                p.compose(task);
            } catch (...) {
                p.set_exception(std::current_exception());
            }
        });
        return f;
    }


    ////////////////////////////////////////////////////////////////////////////////


    namespace
    {
        template<class T>
        struct function_traits : function_traits<decltype(&T::operator())> {
        };

        template<class R, class... Args>
        struct function_traits<R(Args...)> { // function type
            using ret_type  = R;
            using arg_types = std::tuple<Args...>;
        };
        
        template<class R, class... Args>
        struct function_traits<R (*)(Args...)> { // function pointer
            using ret_type  = R;
            using arg_types = std::tuple<Args...>;
        };

        template<class R, class... Args>
        struct function_traits<std::function<R(Args...)>> {
            using ret_type  = R;
            using arg_types = std::tuple<Args...>;
        };
        
        template<class T, class R, class... Args>
        struct function_traits<R (T::*)(Args...)> { // member func ptr
            using ret_type  = R;
            using arg_types = std::tuple<Args...>;
        };

        template<class T, class R, class... Args>
        struct function_traits<R (T::*)(Args...) const> {  // const member func ptr
            using ret_type  = R;
            using arg_types = std::tuple<Args...>;
        };

        template<class T>
        using ret_type = typename function_traits<T>::ret_type;

        template<class T>
        using first_arg_type = typename std::tuple_element<0, typename function_traits<T>::arg_types>::type;
    
        template<class Future>
        using future_type = std::decay_t<decltype(std::declval<Future>().get())>;
    }


    ////////////////////////////////////////////////////////////////////////////////


    template<class T> class NODISCARD cfuture : public std::shared_future<T>
    {
        using super = std::shared_future<T>;
    public:
        cfuture() noexcept = default;
        cfuture(std::future<T>&& f) noexcept : super{std::move(f)}
        {
        }
        cfuture(std::shared_future<T>&& f) noexcept : super{std::move(f)}
        {
        }
        cfuture(cfuture&& f) noexcept : super{std::move(f)}
        {
        }
        cfuture(const cfuture& f) noexcept : super{f}
        {
        }
        cfuture& operator=(std::future<T>&& f) noexcept
        {
            super::operator=(std::move(f));
            return *this;
        }
        cfuture& operator=(std::shared_future<T>&& f) noexcept
        {
            super::operator=(std::move(f));
            return *this;
        }
        cfuture& operator=(cfuture&& f) noexcept
        {
            super::operator=(std::move(f));
            return *this;
        }
        cfuture& operator=(const cfuture& f) noexcept
        {
            super::operator=(f);
            return *this;
        }
        ~cfuture() noexcept // always block if future is still incomplete
        {
            if (this->valid())
                this->wait();
        }

        // downcast chain cfuture<T> to cfuture<void>
        cfuture<void> then();

        template<class Task>
        cfuture<ret_type<Task>> then(Task&& task)
        {
            return rpp::async_task([f=std::move(*this), move_args(task)]() mutable {
                return task(f.get());
            });
        }

        template<class Task, class ExceptHA>
        cfuture<ret_type<Task>> then(Task&& task, ExceptHA&& exhA)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            return async_task([f=std::move(*this), move_args(task, exhA)]() mutable {
                try { return task(f.get()); } 
                catch (ExceptA& a) { return exhA(a); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB>
        cfuture<ret_type<Task>> then(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            return async_task([f=std::move(*this), move_args(task, exhA, exhB)]() mutable {
                try { return task(f.get()); }
                catch (ExceptA& a) { return exhA(a); }
                catch (ExceptB& b) { return exhB(b); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB, class ExceptHC>
        cfuture<ret_type<Task>> then(Task task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            return async_task([f=std::move(*this), move_args(task, exhA, exhB, exhC)]() mutable {
                try { return task(f.get()); }
                catch (ExceptA& a) { return exhA(a); }
                catch (ExceptB& b) { return exhB(b); }
                catch (ExceptC& c) { return exhC(c); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB, class ExceptHC, class ExceptHD>
        cfuture<ret_type<Task>> then(Task task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC, ExceptHD&& exhD)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            using ExceptD = first_arg_type<ExceptHD>;
            return async_task([f=std::move(*this), move_args(task, exhA, exhB, exhC, exhD)]() mutable {
                try { return task(f.get()); }
                catch (ExceptA& a) { return exhA(a); }
                catch (ExceptB& b) { return exhB(b); }
                catch (ExceptC& c) { return exhC(c); }
                catch (ExceptD& d) { return exhD(d); }
            });
        }

        // similar to .then(), but doesn't return a future
        template<class Task>
        void continue_with(Task&& task)
        {
            rpp::parallel_task([f=std::move(*this), move_args(task)]() mutable {
                (void)task(f.get());
            });
        }

        template<class Task, class ExceptHA>
        void continue_with(Task&& task, ExceptHA&& exhA)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA)]() mutable {
                try { (void)task(f.get()); }
                catch (ExceptA& a) { (void)exhA(a); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB>
        void continue_with(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA, exhB)]() mutable {
                try { (void)task(f.get()); }
                catch (ExceptA& a) { (void)exhA(a); }
                catch (ExceptB& b) { (void)exhB(b); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB, class ExceptHC>
        void continue_with(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA, exhB, exhC)]() mutable {
                try { (void)task(f.get()); }
                catch (ExceptA& a) { (void)exhA(a); }
                catch (ExceptB& b) { (void)exhB(b); }
                catch (ExceptC& c) { (void)exhC(c); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB, class ExceptHC, class ExceptHD>
        void continue_with(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC, ExceptHD&& exhD)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            using ExceptD = first_arg_type<ExceptHD>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA, exhB, exhC, exhD)]() mutable {
                try { (void)task(f.get()); }
                catch (ExceptA& a) { (void)exhA(a); }
                catch (ExceptB& b) { (void)exhB(b); }
                catch (ExceptC& c) { (void)exhC(c); }
                catch (ExceptD& d) { (void)exhD(d); }
            });
        }

        // abandons this future and prevents any waiting in destructor
        void detach()
        {
            if (this->valid()) rpp::parallel_task([f=std::move(*this)]() mutable {
                try { (void)f.get(); }
                catch (...) {}
            });
        }
    };


    template<> class NODISCARD cfuture<void> : public std::shared_future<void>
    {
        using super = shared_future<void>;
    public:
        cfuture() noexcept = default;
        cfuture(std::future<void>&& f) noexcept : super{std::move(f)}
        {
        }
        cfuture(shared_future<void>&& f) noexcept : super{std::move(f)}
        {
        }
        cfuture(cfuture&& f) noexcept : super{std::move(f)}
        {
        }
        cfuture(const cfuture& f) noexcept : super{f} // NOLINT
        {
        }
        cfuture& operator=(std::future<void>&& f) noexcept
        {
            super::operator=(std::move(f));
            return *this;
        }
        cfuture& operator=(shared_future<void>&& f) noexcept
        {
            super::operator=(std::move(f));
            return *this;
        }
        cfuture& operator=(cfuture&& f) noexcept
        {
            super::operator=(std::move(f));
            return *this;
        }
        cfuture& operator=(const cfuture& f) noexcept
        {
            super::operator=(f);
            return *this;
        }
        ~cfuture() noexcept // always block if future is still incomplete
        {
            if (this->valid())
                this->wait();
        }

        cfuture<void> then() { return { std::move(*this) }; }

        template<class Task>
        cfuture<ret_type<Task>> then(Task&& task)
        {
            return rpp::async_task([f=std::move(*this), move_args(task)]() mutable {
                f.get();
                return task();
            });
        }

        template<class Task, class ExceptHA>
        cfuture<ret_type<Task>> then(Task&& task, ExceptHA&& exhA)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            return async_task([f=std::move(*this), move_args(task, exhA)]() mutable {
                try { f.get(); return task(); }
                catch (ExceptA& a) { return exhA(a); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB>
        cfuture<ret_type<Task>> then(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            return async_task([f=std::move(*this), move_args(task, exhA, exhB)]() mutable {
                try { f.get(); return task(); }
                catch (ExceptA& a) { return exhA(a); }
                catch (ExceptB& b) { return exhB(b); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB, class ExceptHC>
        cfuture<ret_type<Task>> then(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            return async_task([f=std::move(*this), move_args(task, exhA, exhB, exhC)]() mutable {
                try { f.get(); return task(); }
                catch (ExceptA& a) { return exhA(a); }
                catch (ExceptB& b) { return exhB(b); }
                catch (ExceptC& c) { return exhC(c); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB, class ExceptHC, class ExceptHD>
        cfuture<ret_type<Task>> then(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC, ExceptHD&& exhD)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            using ExceptD = first_arg_type<ExceptHD>;
            return async_task([f=std::move(*this), move_args(task, exhA, exhB, exhC, exhD)]() mutable {
                try { f.get(); return task(); }
                catch (ExceptA& a) { return exhA(a); }
                catch (ExceptB& b) { return exhB(b); }
                catch (ExceptC& c) { return exhC(c); }
                catch (ExceptD& d) { return exhD(d); }
            });
        }

        // similar to .then(), but doesn't return a future
        template<class Task>
        void continue_with(Task&& task)
        {
            rpp::parallel_task([f=std::move(*this), move_args(task)]() mutable {
                f.get();
                (void)task();
            });
        }

        template<class Task, class ExceptHA>
        void continue_with(Task&& task, ExceptHA&& exhA)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA)]() mutable {
                try { f.get(); (void)task(); }
                catch (ExceptA& a) { (void)exhA(a); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB>
        void continue_with(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA, exhB)]() mutable {
                try { f.get(); (void)task(); }
                catch (ExceptA& a) { (void)exhA(a); }
                catch (ExceptB& b) { (void)exhB(b); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB, class ExceptHC>
        void continue_with(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA, exhB, exhC)]() mutable {
                try { f.get(); (void)task(); }
                catch (ExceptA& a) { (void)exhA(a); }
                catch (ExceptB& b) { (void)exhB(b); }
                catch (ExceptC& c) { (void)exhC(c); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB, class ExceptHC, class ExceptHD>
        void continue_with(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC, ExceptHD&& exhD)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            using ExceptD = first_arg_type<ExceptHD>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA, exhB, exhC, exhD)]() mutable {
                try { f.get(); (void)task(); }
                catch (ExceptA& a) { (void)exhA(a); }
                catch (ExceptB& b) { (void)exhB(b); }
                catch (ExceptC& c) { (void)exhC(c); }
                catch (ExceptD& d) { (void)exhD(d); }
            });
        }

        // abandons this future and prevents any waiting in destructor
        void detach()
        {
            if (this->valid()) rpp::parallel_task([f=std::move(*this)]() mutable {
                try { f.get(); } catch (...) {}
            });
        }
    };

    template<class T> cfuture<void> cfuture<T>::then()
    {
        return rpp::async_task([f=std::move(*this)]() mutable {
            (void)f.get();
        });
    }


    template<class T> cfuture<T> make_ready_future(T&& value)
    {
        std::promise<T> p;
        p.set_value(std::move(value));
        return p.get_future();
    }

    inline cfuture<void> make_ready_future()
    {
        std::promise<void> p;
        p.set_value();
        return p.get_future();
    }

    template<class T, class E> cfuture<T> make_exceptional_future(E&& e)
    {
        std::promise<T> p;
        p.set_exception(std::make_exception_ptr(std::forward<E>(e)));
        return p.get_future();
    }
    
    template<class T> void wait_all(const std::vector<cfuture<T>>& vf)
    {
        for (const cfuture<T>& f : vf)
            f.wait();
    }
    
    template<class T> std::vector<T> get_all(const std::vector<cfuture<T>>& vf)
    {
        std::vector<T> all;
        all.reserve(vf.size());
        for (const cfuture<T>& f : vf)
            all.emplace_back(f.get());
        return all;
    }
    
    /**
     * Used to launch multiple parallel tasks and then gather
     * the results. It assumes that launcher already started its
     * own future task, which makes it more flexible.
     * @code
     * vector<Result> results = rpp::get_tasks(DataList,
     *     [&](SomeData& data) {
     *         return rpp::async_task([&]{
     *             return HeavyComputation(data);
     *         };
     *     });
     * @endcode
     */
    template<class U, class Launcher>
    auto get_tasks(std::vector<U>& items, const Launcher& futureLauncher)
        -> std::vector< future_type<ret_type<Launcher>> >
    {
        using T = future_type<ret_type<Launcher>>;
        std::vector<cfuture<T>> futures;
        futures.reserve(items.size());
        
        for (U& item : items)
            futures.emplace_back(futureLauncher(item));
        
        return get_all(futures);
    }

    /**
     * Used to launch multiple parallel tasks and then gather
     * the results. It uses rpp::async_task to launch the futures.
     * @code
     * vector<Result> results = rpp::get_async_tasks(DataList,
     *     [&](SomeData& data) {
     *          return HeavyComputation(data);
     *     });
     * @endcode
     */
    template<class U, class Callback>
    auto get_async_tasks(std::vector<U>& items, const Callback& futureCallback)
        -> std::vector< ret_type<Callback> >
    {
        using T = ret_type<Callback>;
        std::vector<cfuture<T>> futures;
        futures.reserve(items.size());
        
        for (U& item : items)
            futures.emplace_back(rpp::async_task([&] {
                return futureCallback(item);
            }));
        
        return get_all(futures);
    }
    
    /**
     * Used to launch multiple parallel tasks and then wait for
     * the results. It assumes that launcher already started its
     * own future task, which makes it more flexible.
     * @code
     * rpp::run_tasks(DataList, [&](SomeData& data) {
     *      return rpp::async_task([&]{
     *          HeavyComputation(data);
     *      };
     *  });
     * @endcode
     */
    template<class U, class Launcher>
    void run_tasks(std::vector<U>& items, const Launcher& futureLauncher)
    {
        std::vector<cfuture<void>> futures;
        futures.reserve(items.size());
        
        for (U& item : items)
            futures.emplace_back(futureLauncher(item));
        
        return wait_all(futures);
    }
}
