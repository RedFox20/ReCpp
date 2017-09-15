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
 *     delegate<void(int)> fn;
 *     fn.reset();               // clear delegate (uninitialize)
 *     if (fn) fn(42);           // call if initialized
 *
 *  -) Regular function
 *     delegate<void(int)> fn = &func;
 *     fn(42);
 *
 *  -) Member function
 *     delegate<void(int)> fn(myClass, &MyClass::func);  // construct
 *     fn.reset(myClass, &MyClass::func);                // or reset
 *     fn(42);
 *
 *  -) Lambdas:
 *     delegate<void(int)> fn = [](int a) { cout << a << endl; };
 *     fn(42);
 *
 *  -) Functors:
 *     delegate<bool(int,int)> comparer = std::less<int>();
 *     bool result = comparer(37, 42);
 *
 *  -) Events:
 *
 *     event<void(int,int)> on_mouse_move;
 *     on_mouse_move += &scene_mousemove;   // register events
 *     on_mouse_move += &gui_mouse_handler; 
 *     on_mouse_move(deltaX, deltaY);       // invoke multicast delegate (event)
 *     ...
 *     on_mouse_move -= &scene_mousemove;   // unregister existing event
 *     on_mouse_move.clear();               // unregister all
 */
#include <cstdlib> // malloc/free for event<()>
#include <cstring> // memmove for event<()>
#include <type_traits> // std::decay_t<>
#include <cstdio>

namespace rpp
{
    using namespace std; // we love std; you should too.

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
    template<class Func> struct delegate;
    template<class Ret, class... Args> struct delegate<Ret(Args...)>
    {
        // for regular and member function calls
        typedef Ret ret_type;
        typedef Ret(*func_type)(Args...);
        #ifdef _MSC_VER // VC++
            typedef Ret (__thiscall *memb_type)(void*, Args...);
            struct dummy {};
            typedef Ret (dummy::*dummy_type)(Args...);
        #elif defined(__GNUG__) // G++ and clang
            #if __ARM_ARCH // no __thiscall for ARM
                typedef Ret (*memb_type)(void*, Args...);
            #else
                typedef Ret (__thiscall *memb_type)(void*, Args...);
            #endif
            struct dummy {};
            typedef Ret (dummy::*dummy_type)(void*, Args...);
        #endif


        // for functors/lambdas and to ensure delegate copy works
        struct callable
        {
            virtual ~callable() noexcept {}
            virtual void copy(void* dst) const noexcept { }
            virtual Ret invoke(Args&&... args) = 0;
            virtual bool equals(const callable* c) const noexcept = 0;
        };
        struct function : callable
        {
            func_type ptr;
            function(func_type f) noexcept : ptr(f) {}
            void copy(void* dst) const noexcept override
            {
                new (dst) function(ptr);
            }
            Ret invoke(Args&&... args) override
            {
                return (Ret)ptr((Args&&)args...);
            }
            bool equals(const callable* c) const noexcept override
            {
                return ptr == ((function*)c)->ptr;
            }
        };
        struct member : callable
        {
            union {
                memb_type ptr;
                dummy_type dptr; // cast helper
            };
            void* obj;
            member(void* o, memb_type  f) noexcept : ptr(f),  obj(o) {}
            member(void* o, dummy_type f) noexcept : dptr(f), obj(o) {}
            void copy(void* dst) const noexcept override
            {
                new (dst) member(obj, ptr);
            }
            Ret invoke(Args&&... args) override
            {
                return (Ret)ptr(obj, (Args&&)args...);
            }
            bool equals(const callable* c) const noexcept override
            {
                return ptr == ((member*)c)->ptr && obj == ((member*)c)->obj;
            }
        };
        template<class FUNC> struct functor : callable
        {
            // some very basic small Functor optimization applied here:
            // @note The data / ftor fields are never compared for functor
            union {
                void* data[3];
                FUNC* ftor;
            };
            static const int SZ = 3 * sizeof(void*);
            functor(){}
            functor(FUNC&& fn)
            {
                if (sizeof(FUNC) <= SZ)  new (data) FUNC((FUNC&&)fn);
                else                     ftor = new FUNC((FUNC&&)fn);
            }
            functor(const FUNC& fn) noexcept
            {
                if (sizeof(FUNC) <= SZ)  new (data) FUNC(fn);
                else                     ftor = new FUNC(fn);
            }
            ~functor() noexcept override
            {
                static FUNC* lastDeleted;
                if (sizeof(FUNC) <= SZ)  (*(FUNC*)data).~FUNC();
                else {
                    if (lastDeleted == ftor) {
                        fprintf(stderr, "Aaaargh! Double delete of %p!\n", ftor);
                        return;
                    }
                    lastDeleted = ftor;
                    fprintf(stderr, "DELETE %p (%d bytes)\n", ftor, (int)sizeof(FUNC));
                    delete ftor;
                }
            }
            void copy(void* dst) const noexcept override
            {
                new (dst) functor(*ftor);
            }
            Ret invoke(Args&&... args) override
            {
                FUNC& fn = (sizeof(FUNC) <= SZ) ? *(FUNC*)data : *ftor;
                return (Ret)fn((Args&&)args...);
            }
            bool equals(const callable* c) const noexcept override
            {
                return ftor == ((functor*)c)->ftor;
            }
        };


        // hacked polymorphic (callable) container
        void* data[4];
        using pcallable = callable*;


        /** @brief Default constructor */
        delegate() noexcept
        {
            *data = nullptr; // only init what we need to
        }
        /** @brief Regular function constructor */
        delegate(func_type func) noexcept
        {
            if (func) new (data) function(func); // placement new into data buffer
            else *data = nullptr;
        }
        /** @brief Handles functor copy cleanup */
        ~delegate() noexcept
        {
            if (*data) {
                pcallable(data)->~callable();
            }
        }
        

        /** @brief Creates a copy of the delegate */
        delegate(const delegate& d) noexcept
        {
            pcallable(d.data)->copy(data);
        }
        /** @brief Assigns a copy of the delegate */
        delegate& operator=(const delegate& d) noexcept
        {
            if (this != &d)
            {
                this->~delegate();
                pcallable(d.data)->copy(data);
            }
            return *this;
        }
        /** @brief Forward reference initialization */
        delegate(delegate&& d) noexcept // move
        {
            data[0] = d.data[0];
            data[1] = d.data[1];
            data[2] = d.data[2];
            data[3] = d.data[3];
            d.data[0] = nullptr;
        }
        /** @brief Forward reference assignment (swap) */
        delegate& operator=(delegate&& d) noexcept
        {
            void* d0 = data[0], *d1 = data[1], *d2 = data[2], *d3 = data[3];
            data[0] = d.data[0];
            data[1] = d.data[1];
            data[2] = d.data[2];
            data[3] = d.data[3];
            d.data[0] = d0, d.data[1] = d1, d.data[2] = d2, d.data[3] = d3;
            return *this;
        }



        /**
         * @brief Object member function constructor (from pointer)
         * @example delegate<void(int)> d(&myClass, &MyClass::func);
         */
        template<class IClass, class FClass> delegate(IClass* obj, Ret (FClass::*membfunc)(Args...)) noexcept
        {
            if (obj && membfunc)
            #ifdef _MSC_BUILD // VC++
                new (data) member(obj, (dummy_type)membfunc);
            #elif defined(__GNUG__) // G++
                new (data) member(obj, (memb_type)(obj->*membfunc));
            #endif
        }
        /**
         * @brief Object member function constructor (from reference)
         * @example delegate<void(int)> d(myClass, &MyClass::func);
         */
        template<class IClass, class FClass> delegate(IClass& obj, Ret (FClass::*membfunc)(Args...)) noexcept
        {
            if (obj && membfunc)
            #ifdef _MSC_BUILD // VC++
                new (data) member(&obj, (dummy_type)membfunc);
            #elif defined(__GNUG__) // G++
                new (data) member(&obj, (memb_type)(&obj->*membfunc));
            #endif
        }
        /**
         * @brief Functor type constructor for lambdas and old-style functors
         * @example delegate<void(int)> d = [this](int a) { ... };
         * @example delegate<bool(int,int)> compare = std::less<int>();
         */
        template<class Functor> delegate(Functor&& ftor) noexcept
        {
            new (data) typename delegate::functor<std::decay_t<Functor>>((Functor&&)ftor);
        }
        template<class Functor> delegate(const Functor& ftor) noexcept
        {
            new (data) typename delegate::functor<std::decay_t<Functor>>(ftor);
        }



        /**
         * @brief Invoke delegate with specified args list
         */
        //Ret operator()(Args&&... args)
        //{
        //	return (Ret) (*pcallable(data)) ((Args&&)args...);
        //}

        Ret operator()(Args... args)
        {
            return (Ret)pcallable(data)->invoke(forward<Args>(args)...);
        }
        Ret operator()(Args... args) const
        {
            return (Ret)pcallable(data)->invoke(forward<Args>(args)...);
        }
        
        Ret invoke(Args... args)
        {
            return (Ret)pcallable(data)->invoke(forward<Args>(args)...);
        }
        Ret invoke(Args... args) const
        {
            return (Ret)pcallable(data)->invoke(forward<Args>(args)...);
        }

        /** @return true if this delegate is initialize an can be called */
        explicit operator bool() const noexcept
        {
            return *data != nullptr;
        }



        /** @brief Resets the delegate to its default uninitialized state */
        void reset() noexcept
        {
            if (*data) {
                pcallable(data)->~callable();
                *data = nullptr; // only set what we need to
            }
        }
        /** @brief Resets the delegate to point to a function */
        void reset(func_type func) noexcept
        {
            reset();
            if (func) new (data) function(func);
        }
        /** @brief Resets the delegate to point to a member function */
        template<class IClass, class FClass>
        void reset(IClass* obj, Ret (FClass::*membfunc)(Args...)) noexcept
        {
            reset();
            if (obj && membfunc)
            #ifdef _MSC_BUILD // VC++
                new (data) member(obj, (dummy_type)membfunc);
            #elif defined(__GNUG__) // G++
                new (data) member(obj, (memb_type)(obj->*membfunc));
            #endif
        }
        /** @brief Resets the delegate to point to a member function */
        template<class IClass, class FClass>
        void reset(IClass& obj, Ret (FClass::*membfunc)(Args...)) noexcept
        {
            reset();
            if (obj && membfunc)
            #ifdef _MSC_BUILD // VC++
                new (data) member(&obj, (dummy_type)membfunc);
            #elif defined(__GNUG__) // G++
                new (data) member(&obj, (memb_type)(&obj->*membfunc));
            #endif
        }
        /** @brief Resets the delegate to point to a functor or lambda */
        template<class Functor> void reset(Functor&& ftor) noexcept
        {
            reset();
            new (data) typename delegate::functor<decay_t<Functor>>(forward<Functor>(ftor));
        }



        /** @brief Basic operator= shortcuts for reset() */
        delegate& operator=(func_type func)
        {
            reset(func);
            return *this;
        }
        template<class Functor> delegate& operator=(Functor&& functor)
        {
            reset(forward<Functor>(functor));
            return *this;
        }



        /** @brief Basic comparison of delegates */
        bool operator==(const delegate& d) const noexcept
        {
            return data[0] == d.data[0] && // ensure vtables match
                pcallable(data)->equals(pcallable(d.data));
        }
        bool operator!=(const delegate& d) const noexcept
        {
            return data[0] != d.data[0] || // ensure vtables dont match
                !pcallable(data)->equals(pcallable(d.data));
        }
        bool operator==(func_type func) const noexcept
        {
            function vt; // vtable reference object
            return *data == *(void**)&vt && data[1] == func;
        }
        bool operator!=(func_type func) const noexcept
        {
            function vt; // vtable reference object
            return *data != *(void**)&vt || data[1] != func;
        }
        template<class Functor> bool operator==(const Functor& fn) const noexcept
        {
            return *data == *(void**)&fn; // comparing vtables should be enough
        }
        template<class Functor> bool operator!=(const Functor& fn) const noexcept
        {
            return *data != *(void**)&fn; // comparing vtables should be enough
        }



        /** @brief More complex comparison of delegates */
        bool equals(const delegate& d) const noexcept
        {
            return *this == d;
        }
        /** @brief Compares if this delegate is the specified global func */
        bool equals(func_type func) const noexcept
        {
            function vt; // vtable reference object
            return *data == *(void**)&vt && data[1] == func;
        }
        /** @brief Class member function comparison is instance sensitive */
        template<class IClass, class FClass>
        bool equals(IClass* obj, Ret (FClass::*membfunc)(Args...)) const noexcept
        {
            member vt; // vtable reference object
            return *data == *(void**)&vt &&
                 data[1] == (void*)membfunc && data[2] == obj;
        }
        /** @brief Class member function comparison is instance sensitive */
        template<class IClass, class FClass>
        bool equals(IClass& obj, Ret (FClass::*membfunc)(Args...)) const noexcept
        {
            member vt; // vtable reference object
            return *data == *(void**)&vt &&
                 data[1] == (void*)membfunc && data[2] == (void*)&obj;
        }
        /** @brief Comparing Functors by type. Sadly lambdas can't really be
         *         compared in a sensical fashion
         */
        template<class Functor> bool equals() const noexcept
        {
            functor<Functor> vt; // vtable reference object
            return *data == *(void**)&vt; // comparing vtables should be enough
        }
    };






    /**
     * @brief Event class - a delegate container object
     * @note Event class is optimized to have minimal overhead if no events are registered
     *       First registration optimized to reserve only 1 event delegate
     *       Subsequential growth is amortized
     *
     * @example
     *       event<void(int mx, int my)> evt_mouse_move;
     *
     *       evt_mouse_move += &mouse_move;
     *       evt_mouse_move += &gui_mouse_handler;
     *       ...
     *       evt_mouse_move -= &mouse_move;
     *       evt.clear();
     *
     */
    template<class Func> struct event;
    template<class Ret, class... Args> struct event<Ret(Args...)>
    {
        typedef delegate<Ret(Args...)> deleg; // delegate type


        // dynamic data container, actual size is sizeof(container) + sizeof(T)*(capacity-1)
        struct container
        {
            int size;
            int capacity;
            deleg data[1];
        };
        container* ptr; // dynamic delegate array container


        /** @brief Creates an uninitialized event multicast delegate */
        event() noexcept : ptr(nullptr)
        {
        }
        ~event() noexcept
        {
            if (ptr)
            {
                int size = ptr->size;
                auto data = ptr->data;
                for (int i = 0; i < size; ++i)
                    data[i].~deleg();
                free(ptr);
            }
        }
        /** @brief Destructs the event container and frees all used memory */
        void clear() noexcept
        {
            ~event();
            ptr = nullptr;
        }

        /** @return TRUE if there are callable delegates */
        explicit operator bool() const noexcept { return ptr && ptr->size; }
        /** @return Number of currently registered event delegates */
        inline int size() const noexcept { return ptr ? ptr->size : 0; }


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
            int size = ptr->size;
            auto data = ptr->data;
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
        template<class Class> void add(void* obj, Ret (Class::*membfunc)(Args...)) noexcept
        {
            add(deleg(obj, membfunc));
        }
        template<class IClass, class FClass> void add(IClass& obj, Ret (FClass::*membfunc)(Args...)) noexcept
        {
            add(deleg(obj, membfunc));
        }
        template<class Class> void remove(void* obj, Ret(Class::*membfunc)(Args...)) noexcept
        {
            remove(deleg(obj, membfunc));
        }
        template<class IClass, class FClass> void remove(IClass& obj, Ret(FClass::*membfunc)(Args...))
        {
            remove(deleg(obj, membfunc));
        }
        event& operator+=(deleg&& d) noexcept
        {
            add((deleg&&)d);
            return *this;
        }
        event& operator-=(deleg&& d) noexcept
        {
            remove((deleg&&)d);
            return *this;
        }


        /**
         * @brief Invoke all event delegates.
         * @note Regardless of the specified return type, multicast delegate (event)
         *       cannot return any meaningful values. For your own specific behavior
         *       extend this class and implement a suitable version of operator()
         */
        void operator()(Args&&... args) const
        {
            int count = ptr->size;
            auto data = ptr->data;
            for (int i = 0; i < count; ++i)
            {
                data[i]((Args&&)args...);
                //(Ret)(*(T::callable*)data[i].data)(std::forward<Args>(args)...);
            }
        }
    };

} // namespace rpp

#endif // RPP_DELEGATE_H
