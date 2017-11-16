#pragma once
#ifndef RPP_DELEGATE_H
#define RPP_DELEGATE_H
/**
 * Copyright (c) 2015 - Jorma Rebane
 * Usage license: MIT
 *
 * Optimized delegate and event class for VC++ 2013/2015/...
 *
 *
 * Usage examples:
 *
 *  -) Declaring and resetting the delegate
 *  @code
 *     delegate<void(int)> fn;
 *     fn.reset();               // clear delegate (uninitialize)
 *     if (fn) fn(42);           // call if initialized
 *  @endcode
 *
 *  -) Regular function
 *  @code
 *     delegate<void(int)> fn = &func;
 *     fn(42);
 *  @endcode
 *
 *  -) Member function
 *  @code
 *     delegate<void(int)> fn(myClass, &MyClass::func);  // construct
 *     fn.reset(myClass, &MyClass::func);                // or reset
 *     fn(42);
 *  @endcode
 *
 *  -) Lambdas:
 *  @code
 *     delegate<void(int)> fn = [](int a) { cout << a << endl; };
 *     fn(42);
 *  @endcode
 *
 *  -) Functors:
 *  @code
 *     delegate<bool(int,int)> comparer = std::less<int>();
 *     bool result = comparer(37, 42);
 *  @endcode
 *
 *  -) Events:
 *  @code
 *     multicast_delegate<void(int,int)> onMouseMove;
 *     onMouseMove += &scene_mousemove;   // register events
 *     onMouseMove.add(gui, &Gui::MouseMove);
 *     onMouseMove(deltaX, deltaY);       // invoke multicast delegate (event)
 *     ...
 *     onMouseMove -= &scene_mousemove;   // unregister existing event
 *     onMouseMove.clear();               // unregister all
 *  @endcode
 */
#include <cstdlib> // malloc/free for event<()>
#include <cstring> // memmove for event<()>
#include <type_traits> // std::decay_t<>
#include <cstdio>
#include <cassert>

namespace rpp
{
    using namespace std; // we love std; you should too.

    #if INTPTR_MAX == INT64_MAX
        #define RPP_64BIT 1
    #endif
//    #if _WIN64 || __x86_x64__ || __ppc64__ || __arm64__ || __LP64__ || _M_X64 || __amd64__
//        #define RPP_64BIT 1
//    #endif

    /**
     * @brief Function delegate to encapsulate global functions,
     *        instance member functions, lambdas and functors
     * @note Lambda capture has optimizations for small captures,
     *       but big lambda captures allocate a dynamic copy
     *
     * @note All delegate calls result in 2 virtual calls:
     *       callable_proxy(...) => myfuncptr(...)
     *       Which is the same as fastest possible delegates and
     *       inlining results are rather good thanks to this.
     *
     * @example
     *
     *        delegate<int(int,int)> callback = &my_func;
     *        int result = callback(10, 20);
     *
     *        delegate<void(float)> on_move(MyActor, &Actor::update);
     *        on_move(DeltaTime);
     */
    template<class Func> class delegate;
    template<class Ret, class... Args> class delegate<Ret(Args...)>
    {
    public:
        // for regular and member function calls
        using ret_type  = Ret;
        using func_type = Ret (*)(Args...);
        using dtor_type = void (*)(void*);
        using copy_type = void (*)(void*, delegate&);
        struct dummy {};
        #if defined(_MSC_VER)  // VC++
            #if !RPP_64BIT // __thiscall only applies for 32-bit MSVC
                #define THISCALL __thiscall
            #else
                #define THISCALL
            #endif
            using memb_type = Ret (THISCALL*)(void*, Args...);
            using dummy_type = Ret (dummy::*)(Args...);
        #elif defined(__clang__)
            using memb_type = Ret (*)(void*, Args...);
            using dummy_type = Ret (dummy::*)(Args...);
        #elif defined(__GNUG__) // G++
            using memb_type = Ret (*)(void*, Args...);
            using dummy_type = Ret (dummy::*)(void*, Args...);
        #endif
    private:
        union
        {
            func_type   func;
            memb_type  mfunc;
            dummy_type dfunc;
        };
        void*     obj;
        dtor_type destructor;
        copy_type proxy_copy;

    public:
        //////////////////////////////////////////////////////////////////////////////////////

        /** @brief Default constructor */
        delegate() noexcept : func(nullptr), obj(nullptr), destructor(nullptr), proxy_copy(nullptr)
        {
        }
        /** @brief Handles functor copy cleanup */
        ~delegate() noexcept
        {
            if (destructor) {
                destructor(obj);
            }
        }
        
        void copy(delegate& to) const noexcept
        {
            if (destructor) // looks like we have a functor
            {
                proxy_copy(obj, to);
                to.func       = func;
                to.proxy_copy = proxy_copy;
            }
            else
            {
                to.func = func;
                to.obj  = obj;
            }
        }

        /** @brief Creates a copy of the delegate */
        delegate(const delegate& d) noexcept : delegate()
        {
            d.copy(*this);
        }
        /** @brief Assigns a copy of the delegate */
        delegate& operator=(const delegate& d) noexcept
        {
            if (this != &d)
            {
                this->~delegate();
                d.copy(*this);
            }
            return *this;
        }
        /** @brief Forward reference initialization (move) */
        delegate(delegate&& d) noexcept 
            : func      { d.func }, 
              obj       { d.obj  },
              destructor{ d.destructor },
              proxy_copy{ d.proxy_copy }
        {
            d.func       = nullptr;
            d.obj        = nullptr;
            d.destructor = nullptr;
            d.proxy_copy = nullptr;
        }
        /** @brief Forward reference assignment (swap) */
        delegate& operator=(delegate&& d) noexcept
        {
            auto _func       = func;
            auto _obj        = obj;
            auto _destructor = destructor;
            auto _proxy_copy = proxy_copy;
            func       = d.func;
            obj        = d.obj;
            destructor = d.destructor;
            proxy_copy = d.proxy_copy;
            d.func       = _func;
            d.obj        = _obj;
            d.destructor = _destructor;
            d.proxy_copy = _proxy_copy;
            return *this;
        }

        //////////////////////////////////////////////////////////////////////////////////////

        /** @brief Regular function constructor */
        delegate(func_type function) noexcept : func(function), obj(nullptr), destructor(nullptr), proxy_copy(nullptr)
        {
        }

    private:

        template<class IClass, class FClass> void init_method(IClass& inst, Ret (FClass::*method)(Args...)) noexcept
        {
            obj = &inst;
            #if defined(_MSC_VER)  // VC++
                dfunc = (dummy_type)method;
            #elif defined(__clang__)
                dfunc = (dummy_type)method;
            #elif defined(__GNUG__) // G++
                mfunc = (memb_type)(inst.*method);
            #endif
            destructor = nullptr;
            proxy_copy = nullptr;
        }
        template<class IClass, class FClass> void init_method(const IClass& inst, Ret (FClass::*method)(Args...) const) noexcept
        {
            obj = (void*)&inst;
            #if defined(_MSC_VER)  // VC++
                dfunc = (dummy_type)method;
            #elif defined(__clang__)
                dfunc = (dummy_type)method;
            #elif defined(__GNUG__) // G++
                mfunc = (memb_type)(inst.*method);
            #endif
            destructor = nullptr;
            proxy_copy = nullptr;
        }

        union method_helper
        {
            func_type func;
            memb_type mfunc;
            dummy_type dfunc;
        };

        template<class IClass, class FClass> bool equal_method(IClass& inst, Ret (FClass::*method)(Args...)) noexcept
        {
            method_helper u;
            #if defined(_MSC_VER) || defined(__clang__) // VC++ and clang
                u.dfunc = (dummy_type)method;
            #elif defined(__GNUG__) // G++
                u.mfunc = (memb_type)(inst.*method);
            #endif
            return func == u.func;
        }

        template<class IClass, class FClass> bool equal_method(const IClass& inst, Ret (FClass::*method)(Args...) const) noexcept
        {
            method_helper u;
            #if defined(_MSC_VER) || defined(__clang__) // VC++ and clang
                u.dfunc = (dummy_type)method;
            #elif defined(__GNUG__) // G++
                u.mfunc = (memb_type)(inst.*method);
            #endif
            return func == u.func;
        }

        void init_clear()
        {
            func       = nullptr;
            obj        = nullptr;
            destructor = nullptr;
            proxy_copy = nullptr;
        }

    public:

        /**
         * @brief Object member function constructor (from pointer)
         * @code
         *   delegate<void(int)> d(&myClass, &MyClass::method);
         * @endcode
         */
        template<class IClass, class FClass> delegate(IClass* inst, Ret (FClass::*method)(Args...)) noexcept
        {
            if (inst) init_method(*inst, method);
            else      init_clear();
        }
        template<class IClass, class FClass> delegate(IClass& inst, Ret (FClass::*method)(Args...)) noexcept
        {
            init_method(inst, method);
        }

        /**
         * @brief Object member function constructor (from pointer)
         * @code
         *   delegate<void(int)> d(&myClass, &MyClass::method);
         * @endcode
         */
        template<class IClass, class FClass> delegate(const IClass* inst, Ret (FClass::*method)(Args...) const) noexcept
        {
            if (inst) init_method(*inst, method);
            else      init_clear();
        }
        /**
         * @brief Object member function constructor (from reference)
         * @code
         *   delegate<void(int)> d(myClass, &MyClass::method);
         * @endcode
         */
        template<class IClass, class FClass> delegate(const IClass& inst, Ret (FClass::*method)(Args...) const) noexcept
        {
            init_method(inst, method);
        }

    private:

        template<class Functor> void init_functor(Functor&& ftor)
        {
            typedef typename std::decay<Functor>::type FunctorType;
            init_method(ftor, &FunctorType::operator());

            obj  = new FunctorType(forward<Functor>(ftor));
            destructor = [](void* obj)
            {
                auto* instance = (FunctorType*)obj;
                delete instance;
            };
            proxy_copy = [](void* obj, delegate& dest)
            {
                auto* instance = (FunctorType*)obj;
                dest.reset(*instance);
            };
        }

    public:

        /**
         * @brief Functor type constructor for lambdas and old-style functors
         * @code
         *   delegate<void(int)> d = [this](int a) { ... };
         *   delegate<bool(int,int)> compare = std::less<int>();
         * @endcode
         */
        template<class Functor> delegate(Functor&& ftor) noexcept
        {
            init_functor(forward<Functor>(ftor));
        }
        template<class Functor> delegate(const Functor& ftor) noexcept
        {
            init_functor(ftor);
        }


        //////////////////////////////////////////////////////////////////////////////////////

        
        /** @brief Resets the delegate to point to a function */
        void reset(func_type function) noexcept
        {
            reset();
            func = function;
        }
        /** @brief Resets the delegate to point to a member function */
        template<class IClass, class FClass> void reset(IClass* inst, Ret (FClass::*method)(Args...)) noexcept
        {
            reset();
            if (inst) init_method(*inst, method);
        }
        /** @brief Resets the delegate to point to a member function */
        template<class IClass, class FClass> void reset(IClass& inst, Ret (FClass::*method)(Args...)) noexcept
        {
            reset();
            init_method(inst, method);
        }
        /** @brief Resets the delegate to point to a member function */
        template<class IClass, class FClass> void reset(IClass* inst, Ret (FClass::*method)(Args...) const) noexcept
        {
            reset();
            if (inst) init_method(*inst, method);
        }
        /** @brief Resets the delegate to point to a member function */
        template<class IClass, class FClass> void reset(IClass& inst, Ret (FClass::*method)(Args...) const) noexcept
        {
            reset();
            init_method(inst, method);
        }
        /** @brief Resets the delegate to point to a functor or lambda */
        template<class Functor> void reset(Functor&& ftor) noexcept
        {
            reset();
            init_functor(forward<Functor>(ftor));
        }


        /** @brief Resets the delegate to its default uninitialized state */
        void reset() noexcept
        {
            if (destructor) {
                destructor(obj);
                destructor = nullptr;
                proxy_copy = nullptr;
            }
            func = nullptr;
            obj  = nullptr;
        }


        //////////////////////////////////////////////////////////////////////////////////////


        /**
         * @brief Invoke the delegate with specified args list
         */
        template<class ...XArgs> Ret operator()(XArgs&&... args) const
        {
            if (obj)
            {
            #if defined(_MSC_VER)  // VC++
                auto* inst = (dummy*)obj;
                return (Ret)(inst->*dfunc)(forward<XArgs>(args)...);
            #elif defined(__clang__)
                auto* inst = (dummy*)obj;
                return (Ret)(inst->*dfunc)(forward<XArgs>(args)...);
//                return (Ret)mfunc(obj, forward<XArgs>(args)...);
            #else
                return (Ret)mfunc(obj, forward<XArgs>(args)...);
            #endif
            }
            else
                return (Ret)func(forward<XArgs>(args)...);
        }
        template<class ...XArgs> Ret invoke(XArgs&&... args) const
        {
            if (obj)
            {
            #if defined(_MSC_VER)  // VC++
                auto* inst = (dummy*)obj;
                return (Ret)(inst->*dfunc)(forward<XArgs>(args)...);
            #elif defined(__clang__)
                auto* inst = (dummy*)obj;
                return (Ret)(inst->*dfunc)(forward<XArgs>(args)...);
//                return (Ret)mfunc(obj, forward<XArgs>(args)...);
            #else
                return (Ret)mfunc(obj, forward<XArgs>(args)...);
            #endif
            }
            else
                return (Ret)func(forward<XArgs...>(args)...);
        }

        /** @return true if this delegate is initialize and can be called */
        explicit operator bool() const noexcept
        {
            return func != nullptr;
        }


        /** @brief Basic operator= shortcuts for reset() */
        delegate& operator=(func_type function)
        {
            reset(function);
            return *this;
        }
        template<class Functor> delegate& operator=(Functor&& functor)
        {
            reset(forward<Functor>(functor));
            return *this;
        }


        /** @brief Basic comparison of delegates */
        bool operator==(func_type function) const noexcept { return func == function; }
        bool operator!=(func_type function) const noexcept { return func != function; }
        bool operator==(const delegate& d)  const noexcept { return func == d.func && obj == d.obj; }
        bool operator!=(const delegate& d)  const noexcept { return func != d.func || obj != d.obj; }
        template<class Functor> bool operator==(const Functor& fn) const noexcept
        {
            return obj == &fn && mfunc == &Functor::operator();
        }
        template<class Functor> bool operator!=(const Functor& fn) const noexcept
        {
            return obj != &fn || mfunc != &Functor::operator();
        }


        /** @brief Compares if this delegate is the specified global func */
        bool equals(func_type function) const noexcept { return func == function; }

        /** @brief More complex comparison of delegates */
        bool equals(const delegate& d) const noexcept { return *this == d; }

        /** @brief Class member function comparison is instance sensitive */
        template<class IClass, class FClass> bool equals(IClass* inst, Ret (FClass::*method)(Args...)) const noexcept
        {
            return obj == inst && (!inst || equal_method(*inst, method));
        }
        /** @brief Class member function comparison is instance sensitive */
        template<class IClass, class FClass> bool equals(IClass& inst, Ret (FClass::*method)(Args...)) const noexcept
        {
            return obj == &inst && equal_method(inst, method);
        }

        /** @brief Class member function comparison is instance sensitive */
        template<class IClass, class FClass> bool equals(IClass* inst, Ret (FClass::*method)(Args...) const) const noexcept
        {
            return obj == inst && (!inst || equal_method(*inst, method));
        }
        /** @brief Class member function comparison is instance sensitive */
        template<class IClass, class FClass> bool equals(IClass& inst, Ret (FClass::*method)(Args...) const) const noexcept
        {
            return obj == &inst && equal_method(inst, method);
        }

        /** @brief Comparing Functors by type. Sadly lambdas can't really be
         *         compared in a sensical fashion
         */
        template<class Functor> bool equals() const noexcept
        {
            Functor inst;
            return equal_method(inst, &Functor::operator());
        }
    };






    /**
     * @brief A delegate container object
     * @note Multicast Delegate class is optimized to have minimal overhead if no subscribers are registered
     *       First registration optimized to reserve only 1 event delegate
     *       Subsequential growth is amortized
     *
     * @example
     *       multicast_delegate<int, int> evt_mouse_move;
     *
     *       evt_mouse_move += &mouse_move;
     *       evt_mouse_move += &gui_mouse_handler;
     *       ...
     *       evt_mouse_move -= &mouse_move;
     *       evt_mouse_move.clear();
     *
     */
    template<class... Args> struct multicast_delegate
    {
        typedef delegate<void(Args...)> deleg; // delegate type


        // dynamic data container, actual size is sizeof(container) + sizeof(T)*(capacity-1)
        struct container
        {
            int size;
            int capacity;
            deleg data[1];
        };
        container* ptr; // dynamic delegate array container


        /** @brief Creates an uninitialized event multicast delegate */
        multicast_delegate() noexcept : ptr(nullptr)
        {
        }
        ~multicast_delegate() noexcept
        {
            if (ptr)
            {
                int    size = ptr->size;
                deleg* data = ptr->data;
                for (int i = 0; i < size; ++i)
                    data[i].~deleg();
                free(ptr);
            }
        }
        /** @brief Destructs the event container and frees all used memory */
        void clear() noexcept
        {
            this->~multicast_delegate();
            ptr = nullptr;
        }

        /** @return TRUE if there are callable delegates */
        explicit operator bool() const noexcept { return ptr && ptr->size; }
        /** @return Number of currently registered event delegates */
        int size() const noexcept { return ptr ? ptr->size : 0; }


        /** @brief Registers a new delegate to receive notifications */
        void add(deleg&& d) noexcept
        {
            if (!ptr)
            {
                ptr = (container*)malloc(sizeof(container));
                ptr->size = 0;
                ptr->capacity = 1;
            }
            else if (ptr->size == ptr->capacity)
            {
                ptr->capacity += 3;
                if (int rem = ptr->capacity % 4)
                    ptr->capacity += 4 - rem;
                ptr = (container*)realloc(ptr,
                    sizeof(container) + sizeof(deleg) * (ptr->capacity - 1));
            }
            new (&ptr->data[ptr->size++]) deleg((deleg&&)d);
        }
        /**
         * @brief Unregisters the first matching delegate from this event
         * @note Removing lambdas and functors is somewhat inefficient due to functor copying
         */
        void remove(deleg&& d) noexcept
        {
            int    size = ptr->size;
            deleg* data = ptr->data;
            for (int i = 0; i < size; ++i)
            {
                if (data[i] == d)
                {
                    memmove(&data[i], &data[i + 1],
                        sizeof(deleg)*(size - i - 1)); // unshift
                    --ptr->size;
                    return;
                }
            }
        }


        template<class Class> void add(void* obj, void (Class::*membfunc)(Args...)) noexcept
        {
            add(deleg(obj, membfunc));
        }
        template<class IClass, class FClass> void add(IClass& obj, void (FClass::*membfunc)(Args...)) noexcept
        {
            add(deleg(obj, membfunc));
        }
        template<class Class> void add(void* obj, void (Class::*membfunc)(Args...)const) noexcept
        {
            add(deleg(obj, membfunc));
        }
        template<class IClass, class FClass> void add(IClass& obj, void (FClass::*membfunc)(Args...)const) noexcept
        {
            add(deleg(obj, membfunc));
        }


        template<class Class> void remove(void* obj, void (Class::*membfunc)(Args...)) noexcept
        {
            remove(deleg(obj, membfunc));
        }
        template<class IClass, class FClass> void remove(IClass& obj, void (FClass::*membfunc)(Args...))
        {
            remove(deleg(obj, membfunc));
        }
        template<class Class> void remove(void* obj, void (Class::*membfunc)(Args...)const) noexcept
        {
            remove(deleg(obj, membfunc));
        }
        template<class IClass, class FClass> void remove(IClass& obj, void (FClass::*membfunc)(Args...)const)
        {
            remove(deleg(obj, membfunc));
        }


        multicast_delegate& operator+=(deleg&& d) noexcept
        {
            add((deleg&&)d);
            return *this;
        }
        multicast_delegate& operator-=(deleg&& d) noexcept
        {
            remove((deleg&&)d);
            return *this;
        }


        /**
         * @brief Invoke all subscribed event delegates.
         */
        template<class ...XArgs> void operator()(XArgs&&... args) const
        {
            int    size = ptr->size;
            deleg* data = ptr->data;
            for (int i = 0; i < size; ++i)
            {
                data[i].invoke(forward<XArgs>(args)...);
            }
        }
        template<class ...XArgs> void invoke(XArgs&&... args) const
        {
            int    size = ptr->size;
            deleg* data = ptr->data;
            for (int i = 0; i < size; ++i)
            {
                data[i].invoke(forward<XArgs>(args)...);
            }
        }
    };

} // namespace rpp

#endif // RPP_DELEGATE_H
