#pragma once
/**
 * Copyright (c) 2015-2019, Jorma Rebane
 * Distributed under MIT Software License
 *
 * Optimized delegate and multicast delegate classes
 * designed as a faster alternative for std::function
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
#include <stdexcept> // std::terminate

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

    #define RPP_DELEGATE_DEBUGGING 0
    #if RPP_DELEGATE_DEBUGGING
        #define RPP_DELEGATE_DEBUG(fmt, ...) printf(fmt "\n", __VA_ARGS__)
    #else
        #define RPP_DELEGATE_DEBUG(...)
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
     * @code
     *        delegate<int(int,int)> callback = &my_func;
     *        int result = callback(10, 20);
     *
     *        delegate<void(float)> on_move(MyActor, &Actor::update);
     *        on_move(DeltaTime);
     * @endcode
     */
    template<class Func> class delegate;
    template<class Ret, class... Args> class delegate<Ret(Args...)>
    {
    public:
        // for regular and member function calls
        using ret_type  = Ret;
        using func_type = Ret (*)(Args...);
        using func_noexcept_type = Ret (*)(Args...) noexcept; // needed for template overload resolution
        using dtor_type = void (*)(void*) noexcept;
        using copy_type = void (*)(void*, delegate&) noexcept;

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
        #elif __clang__
            using memb_type  = Ret (*)(void*, Args...);
            using dummy_type = Ret (dummy::*)(Args...);
        #else
            using memb_type  = Ret (*)(void*, Args...);
            using dummy_type = Ret (dummy::*)(void*, Args...);
        #endif
    private:

        union func
        {
            func_type fun;
            memb_type mfunc;
            dummy_type dfunc;
            void* pfunc;
        };

        func f; // the function pointer
        void* obj; // optional instance ptr
        dtor_type destructor; // optional destructor if we allocated obj
        copy_type proxy_copy; // optional copy constructor

    public:
        //////////////////////////////////////////////////////////////////////////////////////

        /** @brief Default constructor */
        delegate() noexcept
            : f{nullptr}, obj{nullptr}, destructor{nullptr}, proxy_copy{nullptr}
        {
        }

        /** @brief Default constructor from nullptr */
        explicit delegate(std::nullptr_t) noexcept
            : f{nullptr}, obj{nullptr}, destructor{nullptr}, proxy_copy{nullptr}
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
                to.f = f;
                to.proxy_copy = proxy_copy;
            }
            else
            {
                to.f = f;
                to.obj = obj;
            }
        }

        /** @brief Creates a copy of the delegate */
        delegate(const delegate& d) noexcept
            : f{nullptr}, obj{nullptr}, destructor{nullptr}, proxy_copy{nullptr}
        {
            d.copy(*this);
        }
        /** @brief Assigns a copy of the delegate */
        delegate& operator=(const delegate& d) noexcept
        {
            if (this != &d)
            {
                d.copy(*this);
            }
            return *this;
        }

        /** @brief Forward reference initialization (move) */
        delegate(delegate&& d) noexcept 
            : f{d.f}, obj{d.obj}, destructor{d.destructor}, proxy_copy{d.proxy_copy}
        {
            d.f.fun = nullptr;
            d.obj = nullptr;
            d.destructor = nullptr;
            d.proxy_copy = nullptr;
        }
        /** @brief Forward reference assignment (swap) */
        delegate& operator=(delegate&& d) noexcept
        {
            auto tmp_f = f;
            auto tmp_obj = obj;
            auto tmp_destructor = destructor;
            auto tmp_proxy_copy = proxy_copy;
            f = d.f;
            obj = d.obj;
            destructor = d.destructor;
            proxy_copy = d.proxy_copy;
            d.f = tmp_f;
            d.obj = tmp_obj;
            d.destructor = tmp_destructor;
            d.proxy_copy = tmp_proxy_copy;
            return *this;
        }

    public:

        ///////////////////////////////////////////////////////////////////////////
        // ------------------------- Master constructor ------------------------ //
        ///////////////////////////////////////////////////////////////////////////

        // not const delegate& or delegate&&
        template<class FunctionType>
        using enable_if_not_delegate_t = std::enable_if_t<  !std::is_same_v<std::decay_t<FunctionType>, delegate>  >;

        template<class Function>
        static constexpr bool is_func_type = std::is_same_v<Function, func_type>;

        /**
         * @brief Master constructor for most delegate types
         *        Matches: functors, lambdas, global functions
         */
        template<class FunctionType, typename = enable_if_not_delegate_t<FunctionType>>
        delegate(FunctionType&& function) noexcept
        {
            using Function = typename std::decay_t<FunctionType>;
            if constexpr ((is_func_type<Function> || std::is_same_v<Function, func_noexcept_type>) ||
                          (is_func_type<Function> && std::is_empty_v<Function>/*stateless lambda*/))
            {
                init_function(function);
            }
            else
            {
                init_functor(std::forward<FunctionType>(function));
            }
        }

        /** @brief Basic operator= shortcut for reset() */
        template<class FunctionType, typename = enable_if_not_delegate_t<FunctionType>>
        DELEGATE_FINLINE delegate& operator=(FunctionType&& function) noexcept
        {
            reset(std::forward<FunctionType>(function));
            return *this;
        }

        /** @brief Generic init which catches: functors, lambdas, global funcs */
        template<class FunctionType, typename = enable_if_not_delegate_t<FunctionType>>
        void reset(FunctionType&& function) noexcept
        {
            reset();
            using Function = typename std::decay_t<FunctionType>;
            if constexpr ((is_func_type<Function> || std::is_same_v<Function, func_noexcept_type>) ||
                          (is_func_type<Function> && std::is_empty_v<Function>/*stateless lambda*/))
            {
                init_function(function);
            }
            else
            {
                init_functor(std::move(function));
            }
        }

    private:
        ///////////////////////////////////////////////////////////////////////////
        // -------------------------- Static Functions ------------------------- //
        ///////////////////////////////////////////////////////////////////////////

        // in order to avoid branching and for better performance in the functor-case
        // we wrap regular functions behind a proxy trampoline
        void init_function(func_type function) noexcept
        {
            RPP_DELEGATE_DEBUG("delegate::init_function(%p)", function);
            if (function)
            {
                // store function as 'this' and call dummy class instance method
                // which will cast 'this' into 'func_type'
                #if (__GNUG__ && __GNUG__ > 5) && !__clang__ // G++
                    #pragma GCC diagnostic push
                    #pragma GCC diagnostic ignored "-Wcast-function-type"
                    f.dfunc = reinterpret_cast<dummy_type>( &dummy::func_proxy );
                    #pragma GCC diagnostic pop
                #else
                    f.dfunc = reinterpret_cast<dummy_type>( &dummy::func_proxy );
                #endif
                obj = reinterpret_cast<void*>(function);
            }
            else
            {
                f.fun = nullptr;
                obj = nullptr;
            }
            destructor = nullptr;
            proxy_copy = nullptr;
        }

    private:

        ///////////////////////////////////////////////////////////////////////////
        // -------------------------- Member Functions ------------------------- //
        ///////////////////////////////////////////////////////////////////////////

        #if _MSC_VER
            union MultiInheritThunk {
                struct {
                    uintptr_t ptr; // function pointer
                    int adj; // this pointer displacement in bytes
                    int padding;
                };
                func f;
            };
            static func devirtualize_mi(const void* inst, void** mi_pmf, void** out_inst = nullptr) noexcept
            {
                MultiInheritThunk* mi_thunk = reinterpret_cast<MultiInheritThunk*>(mi_pmf);
                static_assert(sizeof(dummy_type) == sizeof(MultiInheritThunk::ptr));
                if (out_inst) {
                    *out_inst = (void*)((const uint8_t*)inst + mi_thunk->adj);
                }
                return mi_thunk->f;
            }
            template<class IClass, class FClass, class MethodType>
            static func devirtualize(IClass* inst, MethodType FClass::*method, void** out_inst = nullptr) noexcept
            {
                // for MSVC we always use dfunc (dummy_type) for all delegates, which uses thiscall
                if constexpr (sizeof(method) == sizeof(uintptr_t)*2)
                    return devirtualize_mi(inst, (void**)&method, out_inst);
                else
                {
                    func f; // piecewise init to supports MSVC C++17
                    f.dfunc = reinterpret_cast<dummy_type>(method);
                    return f;
                }
            }
        #elif __clang__ // disable on VC++ clang, enable on all other clang builds
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
            template<class FClass, class MethodType>
            static func devirtualize(const void* inst, MethodType FClass::*method, void** out_inst = nullptr) noexcept
            {
                auto& t = *(struct VCallThunk*)&method;
                if (size_t(t.method) & 1u) { // is_thunk?
                    auto* vtable = (struct VTable*) *(void**)inst;
                    size_t voffset = (t.vtable_index-1)/8;
                    if (out_inst) {
                        *out_inst = (void*)((const uint8_t*)inst + t.this_adjustment);
                    }
                    return { .pfunc = vtable->entries[voffset] }; // resolve thunk
                }
                return func{ .pfunc = t.method }; // not a virtual method thunk
            }
        #elif __GNUG__ // G++
            template<class IClass, class FClass, class MethodType>
            static func devirtualize(IClass* inst, MethodType FClass::*method, void** out_inst = nullptr) noexcept
            {
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wpmf-conversions"
                #pragma GCC diagnostic ignored "-Wpedantic"
                (void)out_inst;
                memb_type mfunc = (memb_type)(inst->*method); // de-virtualize / pmf-conversion
                return func{ .mfunc = mfunc };
                #pragma GCC diagnostic pop
            }
        #endif

        template<class IClass, class FClass, class MethodType> void init_method(IClass* inst, MethodType FClass::*method) noexcept
        {
            obj = inst;
            f = devirtualize(inst, method, &obj);
            RPP_DELEGATE_DEBUG("delegate::init_method(%p,%p)", obj, f.fun);
            destructor = nullptr;
            proxy_copy = nullptr;
        }
        // adapts delegate invoke(Args) into FClass method(TArgs),
        // this ensures `const int&` is correctly passed as `int` and vice-versa
        template<class IClass, class FClass, class MethodType> void init_adapter(IClass* inst, MethodType FClass::*method) noexcept
        {
            RPP_DELEGATE_DEBUG("delegate::init_adapter(%p,%p)", inst, devirtualize(inst, method).pfunc);
            *this = [inst, method](Args... args) -> Ret {
                return (inst->*method)(std::forward<Args>(args)...);
            };
        }
        template<class IClass, class FClass, class MethodType> bool equal_method(IClass* inst, MethodType FClass::*method) const noexcept
        {
            func tmp = devirtualize(inst, method);
            return f.fun == tmp.fun;
        }

    public:

        /**
         * @brief Object member function constructor
         * @code
         *   delegate<void(int)> d(&myClass, &MyClass::method);
         * @endcode
         */
        template<class IClass, class FClass, class...TArgs,
                 std::enable_if_t<std::is_same_v<std::tuple<TArgs...>, std::tuple<Args...>>, int> = 0>
        delegate(IClass* inst, Ret (FClass::*method)(TArgs...))
        {
            if (!inst) throw std::invalid_argument{"delegate ctor: inst is nullptr"};
            init_method(inst, method);
        }

        /**
         * @brief Object member function constructor
         * @code
         *   delegate<void(int)> d(&myClass, &MyClass::method);
         * @endcode
         */
        template<class IClass, class FClass, class...TArgs,
                 std::enable_if_t<std::is_same_v<std::tuple<TArgs...>, std::tuple<Args...>>, int> = 0>
        delegate(const IClass* inst, Ret (FClass::*method)(TArgs...) const)
        {
            if (!inst) throw std::invalid_argument{"delegate ctor: inst is nullptr"};
            using NonConstMethod = Ret (FClass::*)(TArgs...);
            init_method(const_cast<IClass*>(inst), NonConstMethod(method));
        }

        /** @brief Resets the delegate to point to a member function */
        template<class IClass, class FClass> void reset(IClass* inst, Ret (FClass::*method)(Args...)) noexcept
        {
            reset();
            if (inst) init_method(inst, method);
        }
        /** @brief Resets the delegate to point to a member function */
        template<class IClass, class FClass> void reset(const IClass* inst, Ret (FClass::*method)(Args...) const) noexcept
        {
            reset();
            using NonConstMethod = Ret (FClass::*)(Args...);
            if (inst) init_method(const_cast<IClass*>(inst), NonConstMethod(method));
        }

        /**
         * @brief Object member function constructor with adapter when argument types mismatch the delegate
         * @code
         *   delegate<void(int)> d(&myClass, &MyClass::method);
         * @endcode
         */
        template<class IClass, class FClass, class...TArgs,
                 std::enable_if_t<!std::is_same_v<std::tuple<TArgs...>, std::tuple<Args...>>, int> = 0>
        delegate(IClass* inst, Ret (FClass::*method)(TArgs...)) : delegate{}
        {
            if (!inst) throw std::invalid_argument{"delegate ctor: inst is nullptr"};
            init_adapter(inst, method);
        }

        /**
         * @brief Object member function constructor with adapter when argument types mismatch the delegate
         * @code
         *   delegate<void(int)> d(&myClass, &MyClass::method);
         * @endcode
         */
        template<class IClass, class FClass, class...TArgs,
                 std::enable_if_t<!std::is_same_v<std::tuple<TArgs...>, std::tuple<Args...>>, int> = 0>
        delegate(const IClass* inst, Ret (FClass::*method)(TArgs...) const) : delegate{}
        {
            if (!inst) throw std::invalid_argument{"delegate ctor: inst is nullptr"};
            using NonConstMethod = Ret (FClass::*)(TArgs...);
            init_adapter(const_cast<IClass*>(inst), NonConstMethod(method));
        }

    private:

        ///////////////////////////////////////////////////////////////////////////
        // ------------------------------ Functors ----------------------------- //
        ///////////////////////////////////////////////////////////////////////////

    #if _MSC_VER // for MSVC we use thiscall for everything
        template<class FunctorType> struct functor_dummy
        {
            Ret functor_call(Args... args) // may throw
            {
                FunctorType& functor = *reinterpret_cast<FunctorType*>(this);
                return functor(std::forward<Args>(args)...);
            }
        };
    #else // for G++ and Clang++ we use mfunc call for everything
        template<class FunctorType> static Ret functor_call(void* instance, Args... args) // may throw
        {
            FunctorType& functor = *reinterpret_cast<FunctorType*>(instance);
            return functor(std::forward<Args>(args)...);
        }
    #endif

        template<class Functor> void init_functor(Functor&& functor) noexcept
        {
            using FunctorType = typename std::decay<Functor>::type;
        #if _MSC_VER
            f.dfunc = reinterpret_cast<dummy_type>( &functor_dummy<FunctorType>::functor_call );
        #else
            f.mfunc = &functor_call<FunctorType>;
        #endif
            RPP_DELEGATE_DEBUG("delegate::init_functor(%p,%p)", &functor, f.mfunc);
            obj = new FunctorType{ std::forward<Functor>(functor) };
            destructor = &functor_delete<FunctorType>;
            proxy_copy = &functor_copy<FunctorType>;
        }

        template<class FunctorType> static void functor_delete(void* self) noexcept
        {
            auto* instance = static_cast<FunctorType*>(self);
            delete instance;
        }

        template<class FunctorType> static void functor_copy(void* self, delegate& dest) noexcept
        {
            auto* instance = static_cast<FunctorType*>(self);
            dest.reset(*instance);
        }

        template<class Functor> bool equal_functor() const noexcept
        {
            using FunctorType = typename std::decay<Functor>::type;
            func tmp;
        #if _MSC_VER
            tmp.dfunc = reinterpret_cast<dummy_type>( &functor_dummy<FunctorType>::functor_call );
        #else
            tmp.mfunc = &functor_call<FunctorType>;
        #endif
            return f.fun == tmp.fun;
        }


        //////////////////////////////////////////////////////////////////////////////////////


    private:
        void init_clear()
        {
            f.fun = nullptr;
            obj = nullptr;
            destructor = nullptr;
            proxy_copy = nullptr;
        }

    public:
        void reset(const delegate& d) noexcept
        {
            d.copy(*this);
        }
        void reset(delegate&& d) noexcept
        {
            this->operator=(std::move(d));
        }

        /** @brief Resets the delegate to its default uninitialized state */
        void reset() noexcept
        {
            if (destructor) {
                destructor(obj);
                destructor = nullptr;
                proxy_copy = nullptr;
            }
            f.fun = nullptr;
            obj = nullptr;
        }


        //////////////////////////////////////////////////////////////////////////////////////


        /** @return true if this delegate is initialized and can be invoked */
        explicit operator bool() const noexcept { return f.fun != nullptr; }


        /** @return true if this delegate is initialized and can be invoked */
        bool good() const noexcept { return f.fun != nullptr; }

        func_type get_fun() const noexcept { return f.fun; }
        void*     get_obj() const noexcept { return obj; }

        /** @brief Basic comparison of delegates, however delegate()  */
        bool operator==(std::nullptr_t) const noexcept { return f.fun == nullptr; }
        bool operator!=(std::nullptr_t) const noexcept { return f.fun != nullptr; }
        bool operator==(const delegate& d)  const noexcept { return f.fun == d.f.fun && obj == d.obj; }
        bool operator!=(const delegate& d)  const noexcept { return f.fun != d.f.fun || obj != d.obj; }

        /** @brief More complex comparison of delegates */
        bool equals(const delegate& d) const noexcept { return *this == d; }

        /** @brief Class member function comparison is instance sensitive */
        template<class IClass, class FClass> bool equals(IClass* inst, Ret (FClass::*method)(Args...)) const noexcept
        {
            return obj == inst && (!inst || equal_method(inst, method));
        }

        /** @brief Class member function comparison is instance sensitive */
        template<class IClass, class FClass> bool equals(const IClass* inst, Ret (FClass::*method)(Args...) const) const noexcept
        {
            using NonConstMethod = Ret (FClass::*)(Args...);
            return obj == inst && (!inst || equal_method(const_cast<IClass*>(inst), NonConstMethod(method)));
        }

        /** @brief Compares Functor by signature. */
        template<class Functor> bool equals() const noexcept { return equal_functor<Functor>(); }

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
        return (reinterpret_cast<dummy*>(obj)->*f.dfunc)(std::forward<Args>(args)...);
    #else
        return f.mfunc(obj, std::forward<Args>(args)...);
    #endif
    }


    template<class Ret, class... Args> DELEGATE_FINLINE
    Ret delegate<Ret(Args...)>::invoke(Args... args) const
    {
    #if _MSC_VER
        return (reinterpret_cast<dummy*>(obj)->*f.dfunc)(std::forward<Args>(args)...);
    #else
        return f.mfunc(obj, std::forward<Args>(args)...);
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
            clear();
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
            if (ptr)
            {
                int    size = ptr->size;
                deleg* data = ptr->data;
                for (int i = 0; i < size; ++i)
                    data[i].~deleg();
                free(ptr);
                ptr = nullptr;
            }
        }

        /** @return TRUE if there are callable delegates */
        explicit operator bool() const noexcept { return ptr && ptr->size; }
        bool good() const noexcept { return ptr && ptr->size; }
        bool empty() const noexcept { return !ptr || !ptr->size; }

        /** @return Number of currently registered event delegates */
        int size() const noexcept { return ptr ? ptr->size : 0; }

              deleg* begin() noexcept       { return ptr ? ptr->data : nullptr; }
        const deleg* begin() const noexcept { return ptr ? ptr->data : nullptr; }
              deleg* end() noexcept       { return ptr ? ptr->data + ptr->size : nullptr; }
        const deleg* end() const noexcept { return ptr ? ptr->data + ptr->size : nullptr; }

    private:

        void grow() noexcept
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
                auto* p = static_cast<container*>(realloc(reinterpret_cast<void*>(ptr), 
                                                  sizeof(container) + sizeof(deleg) * (ptr->capacity - 1)));
                if (!p) { std::terminate(); }
                ptr = p;
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
            container* c = ptr;
            if (!c) return;

            int    size = c->size;
            deleg* data = c->data;
            for (int i = 0; i < size; ++i)
            {
                if (data[i] == d)
                {
                    --c->size;
                    data[i] = deleg{}; // reset the delegate (dealloc if needed)

                    int unshift = size - (i + 1);
                    if (unshift > 0) { // unshift N elements from end of array if needed
                        memmove(reinterpret_cast<void*>(&data[i]), reinterpret_cast<void*>(&data[i + 1]),
                                sizeof(deleg)*unshift);
                    }
                    return;
                }
            }
        }


        template<class IClass, class FClass> void add(IClass* obj, void (FClass::*method)(Args...))
        {
            add(deleg{obj, method});
        }
        template<class IClass, class FClass> void add(const IClass* obj, void (FClass::*method)(Args...) const)
        {
            add(deleg{obj, method});
        }


        template<class IClass, class FClass> void remove(IClass* obj, void (FClass::*method)(Args...))
        {
            remove(deleg{obj, method});
        }
        template<class IClass, class FClass> void remove(const IClass* obj, void (FClass::*method)(Args...) const)
        {
            remove(deleg{obj, method});
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
        container* c = ptr;
        if (!c) return;
        int    size = c->size;
        deleg* data = c->data;
        for (int i = 0; i < size; ++i)
        {
            data[i](static_cast<multicast_fwd_t<Args>>(args)...);
        }
    }

    template<class... Args>
    void multicast_delegate<Args...>::invoke(Args... args) const
    {
        container* c = ptr;
        if (!c) return;
        int    size = c->size;
        deleg* data = c->data;
        for (int i = 0; i < size; ++i)
        {
            data[i](static_cast<multicast_fwd_t<Args>>(args)...);
        }
    }

} // namespace
