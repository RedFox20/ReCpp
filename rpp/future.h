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
    using std::future;
    using std::shared_future;
    using std::promise;
    using std::exception;
    using std::move;
    using std::vector;

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
                Task t = move(task);
                (void)t;
            }
            std::promise<T>::set_value(move(value));
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
                Task t = move(task);
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
        shared_future<T> f = p.get_future().share();
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
    }


    ////////////////////////////////////////////////////////////////////////////////


    template<class T> class NODISCARD cfuture : public shared_future<T>
    {
        using super = shared_future<T>;
    public:
        cfuture() noexcept = default;
        cfuture(future<T>&& f) noexcept : super(move(f))
        {
        }
        cfuture(shared_future<T>&& f) noexcept : super(move(f))
        {
        }
        cfuture(cfuture&& f) noexcept : super(move(f))
        {
        }
        cfuture(const cfuture& f) noexcept : super(f)
        {
        }
        cfuture& operator=(future<T>&& f) noexcept
        {
            super::operator=(move(f));
            return *this;
        }
        cfuture& operator=(shared_future<T>&& f) noexcept
        {
            super::operator=(move(f));
            return *this;
        }
        cfuture& operator=(cfuture&& f) noexcept
        {
            super::operator=(move(f));
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
            return rpp::async_task([f=move(*this), move_args(task)]() mutable {
                return task(f.get());
            });
        }

        template<class Task, class ExceptHA>
        cfuture<ret_type<Task>> then(Task&& task, ExceptHA&& exhA)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            return async_task([f=move(*this), move_args(task, exhA)]() mutable {
                try { return task(f.get()); } 
                catch (ExceptA& a) { return exhA(a); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB>
        cfuture<ret_type<Task>> then(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            return async_task([f=move(*this), move_args(task, exhA, exhB)]() mutable {
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
            return async_task([f=move(*this), move_args(task, exhA, exhB, exhC)]() mutable {
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
            return async_task([f = move(*this), move_args(task, exhA, exhB, exhC, exhD)]() mutable {
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
            rpp::parallel_task([f=move(*this), move_args(task)]() mutable {
                (void)task(f.get());
            });
        }

        template<class Task, class ExceptHA>
        void continue_with(Task&& task, ExceptHA&& exhA)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            rpp::parallel_task([f=move(*this), move_args(task, exhA)]() mutable {
                try { (void)task(f.get()); }
                catch (ExceptA& a) { (void)exhA(a); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB>
        void continue_with(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            rpp::parallel_task([f=move(*this), move_args(task, exhA, exhB)]() mutable {
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
            rpp::parallel_task([f=move(*this), move_args(task, exhA, exhB, exhC)]() mutable {
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
            rpp::parallel_task([f = move(*this), move_args(task, exhA, exhB, exhC, exhD)]() mutable {
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
            if (this->valid()) rpp::parallel_task([f = move(*this)]() mutable {
                try { (void)f.get(); }
                catch (...) {}
            });
        }
    };


    template<> class NODISCARD cfuture<void> : public shared_future<void>
    {
        using super = shared_future<void>;
    public:
        cfuture() noexcept = default;
        cfuture(future<void>&& f) noexcept : super(move(f))
        {
        }
        cfuture(shared_future<void>&& f) noexcept : super(move(f))
        {
        }
        cfuture(cfuture&& f) noexcept : super(move(f))
        {
        }
        cfuture(const cfuture& f) noexcept : super(f)
        {
        }
        cfuture& operator=(future<void>&& f) noexcept
        {
            super::operator=(move(f));
            return *this;
        }
        cfuture& operator=(shared_future<void>&& f) noexcept
        {
            super::operator=(move(f));
            return *this;
        }
        cfuture& operator=(cfuture&& f) noexcept
        {
            super::operator=(move(f));
            return *this;
        }
        cfuture& operator=(const cfuture& f) noexcept
        {
            super::operator=(move(f));
            return *this;
        }
        ~cfuture() noexcept // always block if future is still incomplete
        {
            if (this->valid())
                this->wait();
        }

        cfuture<void> then() { return { move(*this) }; }

        template<class Task>
        cfuture<ret_type<Task>> then(Task&& task)
        {
            return rpp::async_task([f=move(*this), move_args(task)]() mutable {
                f.get();
                return task();
            });
        }

        template<class Task, class ExceptHA>
        cfuture<ret_type<Task>> then(Task&& task, ExceptHA&& exhA)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            return async_task([f=move(*this), move_args(task, exhA)]() mutable {
                try { f.get(); return task(); }
                catch (ExceptA& a) { return exhA(a); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB>
        cfuture<ret_type<Task>> then(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            return async_task([f=move(*this), move_args(task, exhA, exhB)]() mutable {
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
            return async_task([f=move(*this), move_args(task, exhA, exhB, exhC)]() mutable {
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
            return async_task([f = move(*this), move_args(task, exhA, exhB, exhC, exhD)]() mutable {
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
            rpp::parallel_task([f=move(*this), move_args(task)]() mutable {
                f.get();
                (void)task();
            });
        }

        template<class Task, class ExceptHA>
        void continue_with(Task&& task, ExceptHA&& exhA)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            rpp::parallel_task([f=move(*this), move_args(task, exhA)]() mutable {
                try { f.get(); (void)task(); }
                catch (ExceptA& a) { (void)exhA(a); }
            });
        }

        template<class Task, class ExceptHA, class ExceptHB>
        void continue_with(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB)
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            rpp::parallel_task([f=move(*this), move_args(task, exhA, exhB)]() mutable {
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
            rpp::parallel_task([f=move(*this), move_args(task, exhA, exhB, exhC)]() mutable {
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
            rpp::parallel_task([f = move(*this), move_args(task, exhA, exhB, exhC, exhD)]() mutable {
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
            if (this->valid()) rpp::parallel_task([f = move(*this)]() mutable {
                try { f.get(); } catch (...) {}
            });
        }
    };

    template<class T> cfuture<void> cfuture<T>::then()
    {
        return rpp::async_task([f=move(*this)]() mutable {
            (void)f.get();
        });
    }


    template<class T> cfuture<T> make_ready_future(T&& value)
    {
        promise<T> p;
        p.set_value(move(value));
        return p.get_future();
    }

    inline cfuture<void> make_ready_future()
    {
        promise<void> p;
        p.set_value();
        return p.get_future();
    }

    template<class T, class E> cfuture<T> make_exceptional_future(E&& e)
    {
        promise<T> p;
        p.set_exception(std::make_exception_ptr(std::forward<E>(e)));
        return p.get_future();
    }
    
    template<class T> void wait_all(const vector<cfuture<T>>& vf)
    {
        for (const cfuture<T>& f : vf)
            f.wait();
    }
    
    template<class T> vector<T> get_all(const vector<cfuture<T>>& vf)
    {
        vector<T> all;
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
    template<class T, class U, class Launcher>
    vector<T> get_tasks(vector<U>& items, const Launcher& futureLauncher)
    {
        vector<cfuture<T>> futures;
        futures.reserve(items.size());
        
        for (U& item : items)
            futures.emplace_back(futureLauncher(item));
        
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
    void run_tasks(vector<U>& items, const Launcher& futureLauncher)
    {
        vector<cfuture<void>> futures;
        futures.reserve(items.size());
        
        for (U& item : items)
            futures.emplace_back(futureLauncher(item));
        
        return wait_all(futures);
    }
}
