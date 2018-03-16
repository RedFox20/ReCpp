#pragma once
/**
 * Minimal resumable future extensions for C++11 std::future, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 * @note Original implementation description by Herb Sutter at C++ and beyond 2012
 */
#include <memory> // shared_ptr
#include <future>
#if __has_include("thread_pool.h")
    #define CFUTURE_USE_RPP_THREAD_POOL 1
    #include "thread_pool.h"
    #include <type_traits>
#else
    #include <thread>
    #include <functional>
#endif

#ifndef NODISCARD
    #if __clang__
        #if __clang_major__ >= 4 || (__clang_major__ == 3 && __clang_minor__ == 9) // since 3.9
            #define NODISCARD [[nodiscard]]
        #else
            #define NODISCARD // not supported in clang <= 3.8
        #endif
    #else
        #define NODISCARD [[nodiscard]]
    #endif
#endif

#ifndef FINLINE
#  ifdef _MSC_VER
#    define FINLINE __forceinline
#  elif __APPLE__
#    define FINLINE inline __attribute__((always_inline))
#  else
#    define FINLINE __attribute__((always_inline))
#  endif
#endif

namespace rpp
{
    using std::future;
    using std::shared_future;
    using std::promise;
    using std::exception;
    using std::move;

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


#if CFUTURE_USE_RPP_THREAD_POOL
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
#else // std::async is incredibly slower
    template<class Task>
    auto async_task(Task&& task) -> cfuture<decltype(task())>
    {
        return std::async(std::launch::async, [move_args(task)]() mutable
        {
            return task();
        });
    }
#endif


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
        cfuture(future<T>&& f) noexcept : super(move(f))
        {
        }
        cfuture(shared_future<T>&& f) noexcept : super(move(f))
        {
        }
        cfuture(cfuture&& f) noexcept : super(move(f))
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
        cfuture<ret_type<Task>> then(Task task, ExceptHA exhA, ExceptHB exhB, ExceptHC exhC)
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
    };


    template<> class NODISCARD cfuture<void> : public shared_future<void>
    {
        using super = shared_future<void>;
    public:
        cfuture(future<void>&& f) noexcept : super(move(f))
        {
        }
        cfuture(shared_future<void>&& f) noexcept : super(move(f))
        {
        }
        cfuture(cfuture&& f) noexcept : super(move(f))
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

    template<class T, class E> cfuture<T> make_exceptional_future(E&& e)
    {
        promise<T> p;
        p.set_exception(std::make_exception_ptr(std::forward<E>(e)));
        return p.get_future();
    }
}
