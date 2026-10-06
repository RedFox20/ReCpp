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
#include "config.h"
#include <cstddef> // size_t, ptrdiff_t
#include <cstdlib> // malloc/free for multicast_delegate
#include <cstring> // memcpy
#include <new> // placement new
#include <type_traits> // std::decay_t<>
#include <utility> // std::forward
#include <exception> // std::terminate
#include <stdexcept> // std::invalid_argument
#include <cstdint> // uint8_t

namespace rpp
{
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

    /**
     * @brief Function delegate to encapsulate global functions,
     *        instance member functions, lambdas and functors
     *
     * @note All delegate calls result in a single virtual call:
     *       caller -> delegate::operator() -> target
     *       Which is the same as fastest possible delegates and
     *       inlining results are rather good thanks to this.
     *
     * @note A trivially copyable functor which fits the inline storage lives inside the delegate,
     *       so a small lambda does not allocate. Every other functor lives on the heap.
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

        #if _MSC_VER  // VC++
            struct dummy {
                // proxy for plain old functions to avoid a branch check in delegate::operator()
                Ret func_proxy(Args... args) {
                    auto func = reinterpret_cast<func_type>(this);
                    return func(std::forward<Args>(args)...);
                }
            };
            #if DELEGATE_32_BIT // __thiscall only applies for 32-bit MSVC
                using memb_type = Ret (__thiscall*)(void*, Args...);
            #else
                using memb_type = Ret (*)(void*, Args...);
            #endif
            using dummy_type = Ret (dummy::*)(Args...);
        #else // G++ and Clang++
            using memb_type = Ret (*)(void*, Args...);
        #endif

    private:

        union func
        {
            func_type fun;
            memb_type mfunc;
        #if _MSC_VER
            dummy_type dfunc;
        #endif
            void* pfunc;
        };

        // destroys a heap functor when `to` is null, else copies it into `to`
        using manager_type = void (*)(void* functor, delegate* to) noexcept;
        static constexpr size_t inline_words = 3;
        static constexpr size_t inline_size = inline_words * sizeof(void*);

        func f; // the function pointer
        void* obj; // the instance, the function, or the functor, which can point into `storage`
        union
        {
            manager_type manager; // owns a heap functor, and stays null for every other kind
            void* words[inline_words]; // always initialized, so copy_bits can copy every byte
            alignas(void*) unsigned char storage[inline_size]; // holds an inline functor, then `obj == storage`
        };

    public:
        //////////////////////////////////////////////////////////////////////////////////////

        /** @brief Default constructor */
        delegate() noexcept
            : f{nullptr}, obj{nullptr}, words{}
        {
        }

        /** @brief Handles functor copy cleanup */
        DELEGATE_FINLINE ~delegate() noexcept
        {
            if (owns_heap()) manager(obj, nullptr);
        }

        /** @brief Copies this delegate into `to`, and frees what `to` held before */
        void copy(delegate& to) const noexcept
        {
            if (this == &to) return; // a self copy has nothing to do, and reset() would free the source
            to.reset();
            if (owns_heap()) manager(obj, &to); // a heap functor clones itself
            else to.copy_bits(*this);
        }

        /** @brief Creates a copy of the delegate */
        delegate(const delegate& d) noexcept : delegate{}
        {
            d.copy(*this);
        }
        /** @brief Assigns a copy of the delegate */
        delegate& operator=(const delegate& d) noexcept
        {
            d.copy(*this);
            return *this;
        }

        /** @brief Forward reference initialization (move) */
        DELEGATE_FINLINE delegate(delegate&& d) noexcept
        {
            copy_bits(d);
            d.clear_bits();
        }
        /** @brief Forward reference assignment (swap) */
        delegate& operator=(delegate&& d) noexcept
        {
            if (this != &d)
            {
                // loads both sides before any store, so the compiler keeps the swap in registers
                void* this_obj = is_inline() ? d.storage : obj; // an inline functor moves into the other storage
                void* d_obj = d.is_inline() ? storage : d.obj;
                func this_f = f, d_f = d.f; // NOLINT(readability-isolate-declaration)
                for (size_t i = 0; i < inline_size; i += sizeof(uintptr_t))
                {
                    uintptr_t this_word, d_word; // NOLINT(readability-isolate-declaration)
                    memcpy(&this_word, storage + i, sizeof(this_word));
                    memcpy(&d_word, d.storage + i, sizeof(d_word));
                    memcpy(storage + i, &d_word, sizeof(d_word));
                    memcpy(d.storage + i, &this_word, sizeof(this_word));
                }
                f = d_f;
                obj = d_obj;
                d.f = this_f;
                d.obj = this_obj;
            }
            return *this;
        }

    private:
        DELEGATE_FINLINE bool is_inline() const noexcept { return obj == static_cast<const void*>(storage); }
        // a null first word ends the test in one load, and an inline functor overlaps that word
        DELEGATE_FINLINE bool owns_heap() const noexcept
        {
            manager_type first_word;
            memcpy(&first_word, storage, sizeof(first_word));
            return first_word && !is_inline();
        }

        // copies every kind except a heap functor bit by bit, which moves the heap pointer
        DELEGATE_FINLINE void copy_bits(const delegate& d) noexcept
        {
            f = d.f;
            memcpy(storage, d.storage, inline_size);
            obj = d.is_inline() ? storage : d.obj;
        }
        DELEGATE_FINLINE void clear_storage() noexcept
        {
            memset(words, 0, sizeof(words));
        }
        DELEGATE_FINLINE void clear_bits() noexcept
        {
            f.fun = nullptr;
            obj = nullptr;
            clear_storage();
        }

    public:

        ///////////////////////////////////////////////////////////////////////////
        // ------------------------- Master constructor ------------------------ //
        ///////////////////////////////////////////////////////////////////////////

        // not const delegate& or delegate&&
        template<class FunctionType>
        static constexpr bool not_copy_ctor = !std::is_same_v<std::decay_t<FunctionType>, delegate>;

        // the member constructors take the method arguments verbatim, and the adapters take the rest
        template<class...TArgs>
        static constexpr bool args_match = std::is_same_v<void(TArgs...), void(Args...)>;

        // a function pointer of the exact signature, which init_function stores without a functor
        template<class Function>
        static constexpr bool is_function_pointer = std::is_same_v<Function, func_type> || std::is_same_v<Function, func_noexcept_type>;

        /**
         * @brief Master constructor for most delegate types
         *        Matches: functors, lambdas, global functions
         */
        template<class FunctionType> requires not_copy_ctor<FunctionType>
        delegate(FunctionType&& function) noexcept
        {
            init(std::forward<FunctionType>(function));
        }

        /** @brief Basic operator= shortcut for reset() */
        template<class FunctionType> requires not_copy_ctor<FunctionType>
        DELEGATE_FINLINE delegate& operator=(FunctionType&& function) noexcept
        {
            reset(std::forward<FunctionType>(function));
            return *this;
        }

        /** @brief Generic init which catches: functors, lambdas, global funcs */
        template<class FunctionType> requires not_copy_ctor<FunctionType>
        void reset(FunctionType&& function) noexcept
        {
            reset();
            init(std::forward<FunctionType>(function));
        }

    private:
        template<class FunctionType> void init(FunctionType&& function) noexcept
        {
            using Function = std::decay_t<FunctionType>;
            if constexpr (std::is_same_v<Function, std::nullptr_t>)
                clear_bits();
            else if constexpr (is_function_pointer<Function>)
                init_function(function);
            else
                init_functor(std::forward<FunctionType>(function));
        }

        ///////////////////////////////////////////////////////////////////////////
        // -------------------------- Static Functions ------------------------- //
        ///////////////////////////////////////////////////////////////////////////

        // in order to avoid branching and for better performance in the functor-case
        // we wrap regular functions behind a proxy trampoline
        void init_function(func_type function) noexcept
        {
            if (!function)
            {
                clear_bits();
                return;
            }
        #if _MSC_VER
            f.dfunc = &dummy::func_proxy;
        #else
            f.mfunc = &function_proxy;
        #endif
            obj = reinterpret_cast<void*>(function);
            clear_storage();
        }

    #if !_MSC_VER
        RPP_CORO_WRAPPER static Ret function_proxy(void* function, Args... args)
        {
            return reinterpret_cast<func_type>(function)(std::forward<Args>(args)...);
        }
    #endif

        ///////////////////////////////////////////////////////////////////////////
        // -------------------------- Member Functions ------------------------- //
        ///////////////////////////////////////////////////////////////////////////

        #if _MSC_VER
            union MultiInheritThunk {
                struct {
                    uintptr_t ptr; // function pointer
                    int adj; // this pointer displacement in bytes
                    int vbindex; // the virtual base table index of a virtual inheritance member pointer
                };
                func f;
            };
            // a virtual base adjustment needs the vbptr offset of the class, which only the compiler knows
            template<class FClass, class MethodType>
            static bool needs_adapter(MethodType FClass::*method) noexcept
            {
                // a data member pointer outgrows an int only in the virtual and the unspecified inheritance models
                if constexpr (sizeof(int FClass::*) == sizeof(int))
                    return false;
                else if constexpr (sizeof(method) == sizeof(MultiInheritThunk)) // the virtual inheritance model
                    return reinterpret_cast<const MultiInheritThunk*>(&method)->vbindex != 0;
                else
                    return true;
            }
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
                func f; // piecewise init to supports MSVC C++17
                if constexpr (sizeof(method) == sizeof(dummy_type))
                    f.dfunc = reinterpret_cast<dummy_type>(method);
                else if constexpr (sizeof(method) <= sizeof(MultiInheritThunk))
                    return devirtualize_mi(inst, (void**)&method, out_inst);
                else
                    f.pfunc = nullptr; // needs_adapter() sends the unspecified model to init_adapter
                return f;
            }
        #else // G++ and Clang++ use the Itanium C++ ABI member pointer {ptr, adj}
            struct VTable {
                void* entries[16]; // size is pseudo, mainly for gdb
            };
            struct VCallThunk {
                void* ptr;     // function address, or the vtable byte offset of a virtual
                std::ptrdiff_t adj; // this adjustment in bytes
            };
            template<class FClass, class MethodType>
            static func devirtualize(const void* inst, MethodType FClass::*method, void** out_inst) noexcept
            {
                static_assert(sizeof(method) == sizeof(VCallThunk));
                VCallThunk t;
                memcpy(&t, &method, sizeof(t));
            #if defined(__arm__) || defined(__aarch64__) || defined(__wasm__) || defined(__mips__)
                // ARM variant: adj is (this_adjustment * 2) | is_virtual, because a function address can be odd
                const bool is_virtual = t.adj & 1;
                const std::ptrdiff_t adjust = t.adj >> 1;
                const size_t voffset = size_t(t.ptr);
            #else
                const bool is_virtual = size_t(t.ptr) & 1u; // ptr is the vtable byte offset + 1
                const std::ptrdiff_t adjust = t.adj;
                const size_t voffset = size_t(t.ptr) - 1;
            #endif
                void* self = (void*)((const uint8_t*)inst + adjust);
                *out_inst = self;
                if (!std::is_polymorphic_v<FClass> || !is_virtual) return func{ .pfunc = t.ptr };
                auto* vtable = (struct VTable*) *(void**)self; // NOLINT
                return func{ .pfunc = vtable->entries[voffset / sizeof(void*)] }; // NOLINT
            }
        #endif

        template<class IClass, class FClass, class MethodType> void init_method(IClass* inst, MethodType FClass::*method) noexcept
        {
            FClass& base = *inst; // a method of a non-primary or a virtual base runs on that base subobject
        #if _MSC_VER
            if (needs_adapter(method))
            {
                init_adapter(&base, method);
                return;
            }
        #endif
            obj = &base;
            f = devirtualize(&base, method, &obj);
            clear_storage();
        }
        // adapts invoke(Args) to method(TArgs), so a `const int&` parameter takes an `int` argument and vice versa
        template<class IClass, class FClass, class MethodType> void init_adapter(IClass* inst, MethodType FClass::*method) noexcept
        {
            init_functor([inst, method](Args... args) -> Ret {
                return (inst->*method)(std::forward<Args>(args)...);
            });
        }
        template<class IClass, class FClass, class MethodType> bool equal_method(IClass* inst, MethodType FClass::*method) const noexcept
        {
            FClass& base = *inst;
            void* self = &base;
            func tmp = devirtualize(&base, method, &self);
            return f.fun == tmp.fun && obj == self;
        }

    public:

        /**
         * @brief Object member function constructor
         * @code
         *   delegate<void(int)> d(&myClass, &MyClass::method);
         * @endcode
         */
        template<class IClass, class FClass, class...TArgs> requires args_match<TArgs...>
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
        template<class IClass, class FClass, class...TArgs> requires args_match<TArgs...>
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
        template<class IClass, class FClass, class...TArgs> requires (!args_match<TArgs...>)
        delegate(IClass* inst, Ret (FClass::*method)(TArgs...))
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
        template<class IClass, class FClass, class...TArgs> requires (!args_match<TArgs...>)
        delegate(const IClass* inst, Ret (FClass::*method)(TArgs...) const)
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
        template<class FunctorType> RPP_CORO_WRAPPER static Ret functor_call(void* instance, Args... args) // may throw
        {
            FunctorType& functor = *reinterpret_cast<FunctorType*>(instance);
            return functor(std::forward<Args>(args)...);
        }
    #endif

        // memcpy relocates an inline functor, so only a trivially copyable one goes there
        template<class F> static constexpr bool fits_inline = std::is_trivially_copyable_v<F>
            && sizeof(F) <= inline_size && alignof(F) <= alignof(void*);

        template<class Functor> void init_functor(Functor&& functor) noexcept
        {
            using FunctorType = std::decay_t<Functor>;
        #if _MSC_VER
            f.dfunc = reinterpret_cast<dummy_type>( &functor_dummy<FunctorType>::functor_call );
        #else
            f.mfunc = &functor_call<FunctorType>;
        #endif
            clear_storage(); // also zeroes the tail and the padding of an inline functor
            if constexpr (fits_inline<FunctorType>)
            {
                obj = new (storage) FunctorType{ std::forward<Functor>(functor) };
            }
            else
            {
                obj = new FunctorType{ std::forward<Functor>(functor) };
                manager = &heap_manager<FunctorType>;
            }
        }

        template<class FunctorType> static void heap_manager(void* functor, delegate* to) noexcept
        {
            auto* instance = static_cast<FunctorType*>(functor);
            if (!to)
                delete instance;
            else if constexpr (std::is_copy_constructible_v<FunctorType>)
                to->init_functor(*instance);
            else // a move-only functor cannot copy, so the copy takes its state
                to->init_functor(std::move(*instance));
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
        DELEGATE_FINLINE void reset() noexcept
        {
            if (owns_heap()) manager(obj, nullptr);
            clear_bits();
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
            return inst ? equal_method(inst, method) : obj == nullptr;
        }

        /** @brief Class member function comparison is instance sensitive */
        template<class IClass, class FClass> bool equals(const IClass* inst, Ret (FClass::*method)(Args...) const) const noexcept
        {
            using NonConstMethod = Ret (FClass::*)(Args...);
            return inst ? equal_method(const_cast<IClass*>(inst), NonConstMethod(method)) : obj == nullptr;
        }

        /** @brief Compares Functor by signature. */
        template<class Functor> bool equals() const noexcept { return equal_functor<Functor>(); }

        //////////////////////////////////////////////////////////////////////////////////////

        /**
         * @brief Invoke the delegate with specified args list
         */
        RPP_CORO_WRAPPER DELEGATE_FINLINE Ret operator()(Args... args) const;
        RPP_CORO_WRAPPER DELEGATE_FINLINE Ret invoke(Args... args) const;
    };


    template<class Ret, class... Args> RPP_CORO_WRAPPER DELEGATE_FINLINE
    Ret delegate<Ret(Args...)>::operator()(Args... args) const
    {
    #if _MSC_VER
        return (reinterpret_cast<dummy*>(obj)->*f.dfunc)(std::forward<Args>(args)...);
    #else
        return f.mfunc(obj, std::forward<Args>(args)...);
    #endif
    }


    template<class Ret, class... Args> RPP_CORO_WRAPPER DELEGATE_FINLINE
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

        // an inline functor points into its own slot, so a move constructs every relocated delegate
        static void relocate(deleg* to, deleg* from) noexcept
        {
            new (to) deleg{static_cast<deleg&&>(*from)};
            from->~deleg();
        }

        void grow() noexcept
        {
            if (!ptr)
            {
                ptr = static_cast<container*>(malloc(sizeof(container)));
                if (!ptr) { std::terminate(); }
                ptr->size = 0;
                ptr->capacity = 1;
            }
            else if (ptr->size == ptr->capacity)
            {
                int capacity = ptr->capacity + 3;
                if (int rem = capacity % 4)
                    capacity += 4 - rem;
                auto* p = static_cast<container*>(malloc(sizeof(container) + sizeof(deleg) * (capacity - 1)));
                if (!p) { std::terminate(); }
                p->size = ptr->size;
                p->capacity = capacity;
                for (int i = 0; i < ptr->size; ++i)
                    relocate(&p->data[i], &ptr->data[i]);
                free(ptr);
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
                    data[i].~deleg();
                    for (int j = i + 1; j < size; ++j)
                        relocate(&data[j - 1], &data[j]);
                    --c->size;
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
