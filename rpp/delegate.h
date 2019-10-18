#pragma once
/**
 * Copyright (c) 2015-2017, Jorma Rebane
 * Distributed under MIT Software License
 *
 * Optimized delegate and event class for VC++ 2013/2015/...
 *
 *
 * Usage examples:
 *
 *  Declaring and resetting the delegate
 *  @code
 *     delegate<void(int)> fn;
 *     fn.reset();               // clear delegate (uninitialize)
 *     if (fn) fn(42);           // call if initialized
 *  @endcode
 *
 *  Regular function
 *  @code
 *     delegate<void(int)> fn = &func;
 *     fn(42);
 *  @endcode
 *
 *  Member function
 *  @code
 *     delegate<void(int)> fn { myClass, &MyClass::func }; // construct
 *     fn.reset(myClass, &MyClass::func);                  // or reset
 *     fn(42);
 *  @endcode
 *
 *  Lambdas:
 *  @code
 *     delegate<void(int)> fn = [](int a) { std::cout << a << endl; };
 *     fn(42);
 *  @endcode
 *
 *  Functor:
 *  @code
 *     delegate<bool(int,int)> compare = std::less<int>();
 *     bool result = compare(37, 42);
 *  @endcode
 *
 *  Multicast Events:
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
#include <utility> // std::forward

namespace rpp
{
    #ifndef DELEGATE_HAS_CXX17
    #  if _MSC_VER
    #    define DELEGATE_HAS_CXX17 (_MSVC_LANG > 201402)
    #  else
    #    define DELEGATE_HAS_CXX17 (__cplusplus >= 201703L)
    #  endif
    #endif

    #ifdef _MSC_VER
    #  ifndef DELEGATE_32_BIT
    #    if INTPTR_MAX != INT64_MAX
    #      define DELEGATE_32_BIT 1
    #    else
    #      define DELEGATE_32_BIT 0
    #    endif
    #  endif
    #endif

    //// @note Some strong hints that some functions are merely wrappers, so should be forced inline
    #ifndef DELEGATE_FINLINE
    #  ifdef _MSC_VER
    #    define DELEGATE_FINLINE __forceinline
    #  else
    #    define DELEGATE_FINLINE inline __attribute__((always_inline))
    #  endif
    #endif


    #if !DELEGATE_HAS_CXX17 && __clang__ // only parse this monstrocity in legacy clang builds
    namespace detail {
        //  from: http://en.cppreference.com/w/cpp/types/result_of, CC-BY-SA
        template <typename F, typename... Args> using invoke_result_ = decltype(invoke(std::declval<F>(), std::declval<Args>()...));
        template <typename Void, typename F, typename... Args> struct invoke_result {};
        template <typename F, typename... Args> struct invoke_result<std::void_t<invoke_result_<F, Args...>>, F, Args...> { using type = invoke_result_<F, Args...>; };
        template <typename Void, typename F, typename... Args> struct is_invocable : std::false_type {};
        template <typename F, typename... Args> struct is_invocable<std::void_t<invoke_result_<F, Args...>>, F, Args...> : std::true_type {};
        template <typename Void, typename R, typename F, typename... Args>
        struct is_invocable_r : std::false_type {};
        template <typename R, typename F, typename... Args>
        struct is_invocable_r<std::void_t<invoke_result_<F, Args...>>, R, F, Args...>
            : std::is_convertible<invoke_result_<F, Args...>, R> {};
    }
    #endif


    /**
     * @brief Function delegate to encapsulate global functions,
     *        instance member functions, lambdas and functors
     *
     * @note All delegate calls result in a single virtual call:
     *       caller -> delegate::operator() -> target
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

        struct dummy {
            // proxy for plain old functions to avoid a branch check in delegate::operator()
            Ret func_proxy(Args... args) {
                auto func = reinterpret_cast<func_type>(this);
                return func(std::forward<Args>(args)...);
            }
        };

        #if _MSC_VER  // VC++
            #if DELEGATE_32_BIT // __thiscall only applies for 32-bit MSVC
                using memb_type = Ret (__thiscall*)(void*, Args...);
            #else
                using memb_type = Ret (*)(void*, Args...);
            #endif
            using dummy_type = Ret (dummy::*)(Args...);
            using func_proxy_type = Ret (*)(void*, Args...);
        #elif __clang__
            using memb_type  = Ret (*)(void*, Args...);
            using dummy_type = Ret (dummy::*)(Args...);
            using func_proxy_type = Ret (*)(void*, Args...);
        #else
            using memb_type  = Ret (*)(void*, Args...);
            using dummy_type = Ret (dummy::*)(void*, Args...);
            using func_proxy_type = Ret (*)(void*, Args...);
        #endif
    private:

        // this is the original function pointer
        union {
            func_type fun;
            memb_type mfunc;
            dummy_type dfunc;
        };
        void* obj;
        dtor_type destructor;
        copy_type proxy_copy;

    public:
        //////////////////////////////////////////////////////////////////////////////////////

        /** @brief Default constructor */
        delegate() noexcept
            : fun{nullptr}, obj{nullptr}, destructor{nullptr}, proxy_copy{nullptr}
        {
        }

        /** @brief Default constructor from nullptr */
        explicit delegate(std::nullptr_t) noexcept
            : fun{nullptr}, obj{nullptr}, destructor{nullptr}, proxy_copy{nullptr}
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
                to.fun        = fun;
                to.proxy_copy = proxy_copy;
            }
            else
            {
                to.fun = fun;
                to.obj = obj;
            }
        }

        /** @brief Creates a copy of the delegate */
        delegate(const delegate& d) noexcept
            : fun{nullptr}, obj{nullptr}, destructor{nullptr}, proxy_copy{nullptr}
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
            : fun{d.fun}, obj{d.obj}, destructor{d.destructor}, proxy_copy{d.proxy_copy}
        {
            d.fun        = nullptr;
            d.obj        = nullptr;
            d.destructor = nullptr;
            d.proxy_copy = nullptr;
        }
        /** @brief Forward reference assignment (swap) */
        delegate& operator=(delegate&& d) noexcept
        {
            auto _func       = fun;
            auto _obj        = obj;
            auto _destructor = destructor;
            auto _proxy_copy = proxy_copy;
            fun        = d.fun;
            obj        = d.obj;
            destructor = d.destructor;
            proxy_copy = d.proxy_copy;
            d.fun        = _func;
            d.obj        = _obj;
            d.destructor = _destructor;
            d.proxy_copy = _proxy_copy;
            return *this;
        }

        //////////////////////////////////////////////////////////////////////////////////////

        /** @brief Regular function constructor */
        /*implicit*/ delegate(func_type function) noexcept
        {
            init_function(function);
        }

    private:

        // in order to avoid branching and for better performance in the functor-case
        // we wrap regular functions behind a proxy trampoline
        void init_function(func_type function) noexcept
        {
            if (function)
            {
                // store function as 'this' and call dummy class instance method
                // which will cast 'this' into 'func_type'
                dfunc = reinterpret_cast<dummy_type>( &dummy::func_proxy );
                obj = reinterpret_cast<void*>(function);
            }
            else
            {
                fun = nullptr;
                obj = nullptr;
            }
            destructor = nullptr;
            proxy_copy = nullptr;
        }

        #if !_MSC_VER && __clang__ // disable on VC++ clang, enable on all other clang builds
            struct VCallThunk {
                union {
                    void* method;
                    size_t vtable_index; // = vindex*2+1 (always odd)
                };
                size_t this_adjustment;
            };
            struct VTable {
                void* entries[16]; // size is pseudo, mainly for gdb
            };
            static dummy_type devirtualize(const void* instance, dummy_type method) noexcept // resolve Thunks into method pointer
            {
                auto& t = *(struct VCallThunk*)&method;
                if (size_t(t.method) & 1u) { // is_thunk?
                    auto* vtable = (struct VTable*) *(void**)instance;
                    size_t voffset = (t.vtable_index-1)/8;
                    union {
                        dummy_type dfunc;
                        void* pfunc;
                    } ch;
                    ch.pfunc = vtable->entries[voffset]; // resolve thunk
                    return ch.dfunc;
                }
                return method; // not a virtual method thunk
            }
        #endif

        template<class IClass, class FClass> void init_method(IClass& inst, Ret (FClass::*method)(Args...)) noexcept
        {
            obj = &inst;
            #if _MSC_VER // VC++ and MSVC clang
                dfunc = reinterpret_cast<dummy_type>(method);
            #elif __clang__
                dfunc = devirtualize(&inst, reinterpret_cast<dummy_type>(method));
            #elif __GNUG__ // G++
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wpmf-conversions"
                #pragma GCC diagnostic ignored "-Wpedantic"
                mfunc = (memb_type)(inst.*method); // de-virtualize / pfm-conversion
                #pragma GCC diagnostic pop
            #endif
            destructor = nullptr;
            proxy_copy = nullptr;
        }
        template<class IClass, class FClass> void init_method(const IClass& inst, Ret (FClass::*method)(Args...) const) noexcept
        {
            obj = &const_cast<IClass&>(inst);
            #if _MSC_VER // VC++ and MSVC clang
                dfunc = reinterpret_cast<dummy_type>(method);
            #elif __clang__
                dfunc = devirtualize(&inst, reinterpret_cast<dummy_type>(method));
            #elif __GNUG__ // G++
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wpmf-conversions"
                #pragma GCC diagnostic ignored "-Wpedantic"
                mfunc = (memb_type)(inst.*method); // de-virtualize / pfm-conversion
                #pragma GCC diagnostic pop
            #endif
            destructor = nullptr;
            proxy_copy = nullptr;
        }

        union comparison_helper
        {
            func_type func;
            memb_type mfunc;
            dummy_type dfunc;
        };
        
        // @note instance is used during de-virtualization
        template<class IClass, class FClass> bool equal_method(IClass& inst, Ret (FClass::*method)(Args...)) noexcept
        {
            comparison_helper u;
            #if _MSC_VER // VC++ and MSVC clang
                (void)inst;
                u.dfunc = reinterpret_cast<dummy_type>(method);
            #elif __clang__ // Unix Clang
                u.dfunc = devirtualize(&inst, reinterpret_cast<dummy_type>(method));
            #elif __GNUG__ // G++
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wpmf-conversions"
                #pragma GCC diagnostic ignored "-Wpedantic"
                u.mfunc = (memb_type)(inst.*method); // de-virtualize / pfm-conversion
                #pragma GCC diagnostic pop
            #endif
            return fun == u.func;
        }
        
        // @note instance is used during de-virtualization
        template<class IClass, class FClass> bool equal_method(const IClass& inst, Ret (FClass::*method)(Args...) const) noexcept
        {
            comparison_helper u;
            #if _MSC_VER // VC++ and MSVC clang
                (void)inst;
                u.dfunc = reinterpret_cast<dummy_type>(method);
            #elif __clang__ // Unix Clang
                u.dfunc = devirtualize(&inst, reinterpret_cast<dummy_type>(method));
            #elif __GNUG__ // G++
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wpmf-conversions"
                #pragma GCC diagnostic ignored "-Wpedantic"
                u.mfunc = (memb_type)(inst.*method); // de-virtualize / pfm-conversion
                #pragma GCC diagnostic pop
            #endif
            return fun == u.func;
        }

        template<class Functor> void init_functor(Functor&& functor) noexcept
        {
            using FunctorType = typename std::decay<Functor>::type;

            dfunc = reinterpret_cast<dummy_type>(&FunctorType::operator());

            obj = new FunctorType(std::forward<Functor>(functor));
            destructor = [](void* self)
            {
                auto* instance = static_cast<FunctorType*>(self);
                delete instance;
            };
            proxy_copy = [](void* self, delegate& dest)
            {
                auto* instance = static_cast<FunctorType*>(self);
                dest.reset(*instance);
            };
        }

        void init_clear()
        {
            fun = nullptr;
            obj = nullptr;
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

        /**
         * Enable delegate initialization overloads only if:
         * 1) not delegate, delegate&& or const delegate&
         * 2) Functor matches `Ret(Args...)`
         */
        template<class Functor>
        using enable_if_callable_t = std::enable_if_t<!std::is_same<std::decay_t<Functor>, delegate>::value // not const delegate& or delegate&&
                                #if DELEGATE_HAS_CXX17
                                    #if _MSC_VER
                                        && std::is_invocable_r_v<Ret, Functor, Args...>>; // matches `Ret(Args...)`
                                    #elif __clang__
                                        && std::__invokable_r<Ret, Functor, Args...>::value>; // matches `Ret(Args...)`
                                    #else
                                        && std::is_invocable_r<Ret, Functor, Args...>::value>; // matches `Ret(Args...)`
                                    #endif
                                #else
                                    #if _MSC_VER
                                        && std::_Is_invocable_r_<Ret, Functor, Args...>::value>; // matches `Ret(Args...)`
                                    #elif __clang__
                                        && std::__invokable_r<Ret, Functor, Args...>::value>; // matches `Ret(Args...)`
                                    #else
                                        && detail::is_invocable_r<Ret, Functor, Args...>::value>; // matches `Ret(Args...)`
                                    #endif
                                #endif
        /**
         * @brief Functor type constructor for lambdas and old-style functors
         * @code
         *   delegate<void(int)> d = [this](int a) { ... };
         *   delegate<bool(int,int)> compare = std::less<int>();
         * @endcode
         */
        template<class Functor, typename = enable_if_callable_t<Functor>>
        /*implicit*/ delegate(Functor&& functor) noexcept
        {
            init_functor(std::move(functor));
        }

        template<class Functor, typename = enable_if_callable_t<Functor>>
        /*implicit*/ delegate(const Functor& functor) noexcept
        {
            init_functor(functor);
        }


        //////////////////////////////////////////////////////////////////////////////////////

        
        /** @brief Resets the delegate to point to a function */
        void reset(func_type function) noexcept
        {
            reset();
            init_function(function);
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
        template<class Functor, typename = enable_if_callable_t<Functor>>
        void reset(Functor&& functor) noexcept
        {
            reset();
            init_functor(std::move(functor));
        }
        template<class Functor, typename = enable_if_callable_t<Functor>>
        void reset(const Functor& functor) noexcept
        {
            reset();
            init_functor(functor);
        }

        /** @brief Resets the delegate to its default uninitialized state */
        void reset() noexcept
        {
            if (destructor) {
                destructor(obj);
                destructor = nullptr;
                proxy_copy = nullptr;
            }
            fun = nullptr;
            obj = nullptr;
        }


        //////////////////////////////////////////////////////////////////////////////////////


        /** @return true if this delegate is initialized and can be invoked */
        explicit operator bool() const noexcept { return fun != nullptr; }


        /** @return true if this delegate is initialized and can be invoked */
        bool good() const noexcept { return fun != nullptr; }

        func_type get_fun() const noexcept { return fun; }
        void*     get_obj() const noexcept { return obj; }

        /** @brief Basic operator= shortcuts for reset() */
        delegate& operator=(func_type function)
        {
            reset(function);
            return *this;
        }
        template<class Functor> delegate& operator=(Functor&& functor)
        {
            reset(std::forward<Functor>(functor));
            return *this;
        }


        /** @brief Basic comparison of delegates */
        bool operator==(std::nullptr_t) const noexcept { return fun == nullptr; }
        bool operator!=(std::nullptr_t) const noexcept { return fun != nullptr; }
        bool operator==(func_type function) const noexcept { return fun == function; }
        bool operator!=(func_type function) const noexcept { return fun != function; }
        bool operator==(const delegate& d)  const noexcept { return fun == d.fun && obj == d.obj; }
        bool operator!=(const delegate& d)  const noexcept { return fun != d.fun || obj != d.obj; }
        template<class Functor> bool operator==(const Functor& fn) const noexcept
        {
            return obj == &fn && mfunc == &Functor::operator();
        }
        template<class Functor> bool operator!=(const Functor& fn) const noexcept
        {
            return obj != &fn || mfunc != &Functor::operator();
        }


        /** @brief Compares if this delegate is the specified global func */
        bool equals(func_type function) const noexcept { return fun == function; }

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

        //////////////////////////////////////////////////////////////////////////////////////

        /**
         * @brief Invoke the delegate with specified args list
         */
        DELEGATE_FINLINE Ret operator()(Args... args) const;
        DELEGATE_FINLINE Ret invoke(Args... args) const;
    };


    template<class Ret, class... Args> DELEGATE_FINLINE
    Ret delegate<Ret(Args...)>::operator()(Args... args) const
    {
    #if _MSC_VER
        return (reinterpret_cast<dummy*>(obj)->*dfunc)(std::forward<Args>(args)...);
    #else
        return mfunc(obj, std::forward<Args>(args)...);
    #endif
    }


    template<class Ret, class... Args> DELEGATE_FINLINE
    Ret delegate<Ret(Args...)>::invoke(Args... args) const
    {
    #if _MSC_VER
        return (reinterpret_cast<dummy*>(obj)->*dfunc)(std::forward<Args>(args)...);
    #else
        return mfunc(obj, std::forward<Args>(args)...);
    #endif
    }



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
        using deleg = delegate<void(Args...)>; // delegate type

        // dynamic data container, actual size is sizeof(container) + sizeof(T)*(capacity-1)
        struct container
        {
            int size;
            int capacity;
            deleg data[1];
        };
        container* ptr; // dynamic delegate array container


        /** @brief Creates an uninitialized event multicast delegate */
        multicast_delegate() noexcept : ptr{nullptr}
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

        multicast_delegate(multicast_delegate&& d) noexcept : ptr{d.ptr}
        {
            d.ptr = nullptr;
        }
        multicast_delegate& operator=(multicast_delegate&& d) noexcept
        {
            std::swap(ptr, d.ptr);
            return *this;
        }

        multicast_delegate(const multicast_delegate& d) noexcept : ptr{nullptr}
        {
            this->operator=(d);
        }
        multicast_delegate& operator=(const multicast_delegate& d) noexcept
        {
            if (this != &d)
            {
                clear();
                for (const deleg& del : d)
                    this->add(del);
            }
            return *this;
        }

        /** @brief Destructs the event container and frees all used memory */
        void clear() noexcept
        {
            this->~multicast_delegate();
            ptr = nullptr;
        }

        /** @return TRUE if there are callable delegates */
        explicit operator bool() const noexcept { return ptr && ptr->size; }
        bool good() const noexcept { return ptr && ptr->size; }
        bool empty() const noexcept { return !ptr || !ptr->size; }

        /** @return Number of currently registered event delegates */
        int size() const noexcept { return ptr ? ptr->size : 0; }

              deleg* begin()       { return ptr ? ptr->data : nullptr; }
        const deleg* begin() const { return ptr ? ptr->data : nullptr; }
              deleg* end()       { return ptr ? ptr->data + ptr->size : nullptr; }
        const deleg* end() const { return ptr ? ptr->data + ptr->size : nullptr; }

    private:

        void grow()
        {
            if (!ptr)
            {
                ptr = static_cast<container*>(malloc(sizeof(container)));
                ptr->size = 0;
                ptr->capacity = 1;
            }
            else if (ptr->size == ptr->capacity)
            {
                ptr->capacity += 3;
                if (int rem = ptr->capacity % 4)
                    ptr->capacity += 4 - rem;
                ptr = static_cast<container*>(realloc(ptr, sizeof(container) + sizeof(deleg) * (ptr->capacity - 1)));
            }
        }

    public:

        /** @brief Registers a new delegate to receive notifications */
        void add(deleg&& d) noexcept
        {
            grow();
            new (&ptr->data[ptr->size++]) deleg{static_cast<deleg&&>(d)};
        }

        void add(const deleg& d) noexcept
        {
            grow();
            new (&ptr->data[ptr->size++]) deleg{d};
        }

        /**
         * @brief Unregisters the first matching delegate from this event
         * @note Removing lambdas and functors is somewhat inefficient due to functor copying
         */
        void remove(const deleg& d) noexcept
        {
            int    size = ptr->size;
            deleg* data = ptr->data;
            for (int i = 0; i < size; ++i)
            {
                if (data[i] == d)
                {
                    memmove(&data[i], &data[i + 1], sizeof(deleg)*(size - i - 1)); // unshift
                    --ptr->size;
                    return;
                }
            }
        }


        template<class Class> void add(void* obj, void (Class::*member_function)(Args...)) noexcept
        {
            add(deleg(obj, member_function));
        }
        template<class IClass, class FClass> void add(IClass& obj, void (FClass::*member_function)(Args...)) noexcept
        {
            add(deleg(obj, member_function));
        }
        template<class Class> void add(void* obj, void (Class::*member_function)(Args...)const) noexcept
        {
            add(deleg(obj, member_function));
        }
        template<class IClass, class FClass> void add(IClass& obj, void (FClass::*member_function)(Args...)const) noexcept
        {
            add(deleg(obj, member_function));
        }


        template<class Class> void remove(void* obj, void (Class::*member_function)(Args...)) noexcept
        {
            remove(deleg(obj, member_function));
        }
        template<class IClass, class FClass> void remove(IClass& obj, void (FClass::*member_function)(Args...))
        {
            remove(deleg(obj, member_function));
        }
        template<class Class> void remove(void* obj, void (Class::*member_function)(Args...)const) noexcept
        {
            remove(deleg(obj, member_function));
        }
        template<class IClass, class FClass> void remove(IClass& obj, void (FClass::*member_function)(Args...)const)
        {
            remove(deleg(obj, member_function));
        }


        multicast_delegate& operator+=(deleg&& d) noexcept
        {
            add(static_cast<deleg&&>(d));
            return *this;
        }
        multicast_delegate& operator+=(const deleg& d) noexcept
        {
            add(d);
            return *this;
        }
        multicast_delegate& operator-=(const deleg& d) noexcept
        {
            remove(d);
            return *this;
        }


        /**
         * @brief Invoke all subscribed event delegates.
         */
        void operator()(Args... args) const;
        void invoke(Args... args) const;
    };

    template<class T> struct multicast_fwd      { using type = const T&; };
    template<class T> struct multicast_fwd<T&>  { using type = T&;       };
    template<class T> struct multicast_fwd<T&&> { using type = T&&;      };
    template<class T> using multicast_fwd_t = typename multicast_fwd<T>::type;

    template<class... Args>
    void multicast_delegate<Args...>::operator()(Args... args) const
    {
        int    size = ptr->size;
        deleg* data = ptr->data;
        for (int i = 0; i < size; ++i)
        {
            data[i](static_cast<multicast_fwd_t<Args>>(args)...);
        }
    }

    template<class... Args>
    void multicast_delegate<Args...>::invoke(Args... args) const
    {
        int    size = ptr->size;
        deleg* data = ptr->data;
        for (int i = 0; i < size; ++i)
        {
            data[i](static_cast<multicast_fwd_t<Args>>(args)...);
        }
    }

} // namespace
