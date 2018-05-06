#pragma once
#include <vector>
#include <cstdlib>
#include <rpp/collections.h>

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

namespace rpp
{
    /**
     * Provides necessary utilities for constructing C++ objects
     * directly from the pool.
     * Deallocation depends on pool reuse strategy
     */
    template<class Pool>
    struct pool_types_constructor
    {
        template<class T> NODISCARD T* allocate()
        {
            Pool* pool = static_cast<Pool*>(this);
            return reinterpret_cast<T*>(pool->allocate(sizeof(T), alignof(T)));
        }

        template<class T, class... Args> NODISCARD T* construct(Args&&...args)
        {
            Pool* pool = static_cast<Pool*>(this);
            T* obj = reinterpret_cast<T*>(pool->allocate(sizeof(T), alignof(T)));
            return new (obj) T{std::forward<Args>(args)...};
        }

        // Calls the destructor on the object
        template<class T> void destruct(T* obj)
        {
            Pool* pool = static_cast<Pool*>(this);
            obj->~T();
            pool->deallocate(obj);
        }

        template<class T> NODISCARD T* allocate_array(int count)
        {
            Pool* pool = static_cast<Pool*>(this);
            return reinterpret_cast<T*>(pool->allocate(sizeof(T) * count, alignof(T)));
        }

        template<class T, class... Args> NODISCARD T* construct_array(int count, const Args&...args)
        {
            Pool* pool = static_cast<Pool*>(this);
            T* arr = reinterpret_cast<T*>(pool->allocate(sizeof(T) * count, alignof(T)));
            for (int i = 0; i < count; ++i)
                new (&arr[i]) T {args...};
            return arr;
        }

        template<class T, class... Args> NODISCARD element_range<T> allocate_range(int count)
        {
            Pool* pool = static_cast<Pool*>(this);
            T* arr = reinterpret_cast<T*>(pool->allocate(sizeof(T) * count, alignof(T)));
            return { arr, count };
        }

        template<class T, class... Args> NODISCARD element_range<T> construct_range(int count, const Args&...args)
        {
            Pool* pool = static_cast<Pool*>(this);
            T* arr = reinterpret_cast<T*>(pool->allocate(sizeof(T) * count, alignof(T)));
            for (int i = 0; i < count; ++i)
                new (&arr[i]) T {args...};
            return { arr, count };
        }
    };

    /**
     * Simplest type of memory pool.
     * Has a predetermined static size.
     * There is no deallocate!
     */
    class linear_static_pool : public pool_types_constructor<linear_static_pool>
    {
        int Remaining;
        char* Buffer;
        char* Ptr;

    public:
        explicit linear_static_pool(int staticBlockSize)
            : Remaining{staticBlockSize}, Buffer{(char*)malloc(staticBlockSize)}, Ptr{Buffer}
        {
            if (int rem = size_t(Buffer) % 16) { // always align Ptr to 16 bytes
                Remaining -= (16 - rem);
                Ptr       += (16 - rem);
            }
        }
        ~linear_static_pool() noexcept
        {
            free(Buffer);
        }

        linear_static_pool(linear_static_pool&& pool) noexcept
            : Remaining{pool.Remaining},
              Buffer{pool.Buffer},
              Ptr{pool.Ptr}
        {
            pool.Remaining = 0;
            pool.Buffer = nullptr;
            pool.Ptr = nullptr;
        }
        linear_static_pool& operator=(linear_static_pool&& pool) noexcept
        {
            std::swap(Remaining, pool.Remaining);
            std::swap(Buffer, pool.Buffer);
            std::swap(Ptr, pool.Ptr);
            return *this;
        }
        
        linear_static_pool(const linear_static_pool&) = delete;
        linear_static_pool& operator=(const linear_static_pool&) = delete;

        int capacity()  const { return int(Ptr - Buffer) + Remaining; }
        int available() const { return Remaining; }

        NODISCARD void* allocate(int size, int align = 8)
        {
            int alignOffset = 0;
            int alignedSize = size;
            if (int rem = size_t(Ptr) % align)
            {
                alignOffset = align - rem;
                alignedSize += alignOffset;
            }

            if (Remaining < alignedSize)
                return nullptr;

            char* mem = Ptr + alignOffset;
            Ptr       += alignedSize;
            Remaining -= alignedSize;
            return mem;
        }

        // There is no deallocate -- by design !!
        void deallocate(void*) { }
    };


    /**
     * A memory pool that allocates by `bump the pointer`
     * and grows in large dynamic chunks.
     * Chunk growth can be controlled
     */
    class linear_dynamic_pool : public pool_types_constructor<linear_dynamic_pool>
    {
        int BlockSize;
        float BlockGrowth;
        std::vector<linear_static_pool> Pools;

    public:

        explicit linear_dynamic_pool(int initialBlockSize = 128*1024, float blockGrowth = 2.0f)
            : BlockSize{initialBlockSize}, BlockGrowth{blockGrowth}
        {
            Pools.emplace_back(initialBlockSize);
        }

        int capacity() const
        {
            int cap = 0;
            for (const linear_static_pool& pool : Pools)
                cap += pool.capacity();
            return cap;
        }

        int available() const
        {
            return Pools.back().available();
        }

        NODISCARD void* allocate(int size, int align = 8)
        {
            linear_static_pool* pool = &Pools.back();
            int available = pool->available();
            if (size > available)
            {
                int newBlockSize = int(BlockSize * BlockGrowth);
                if (size > newBlockSize)
                    return nullptr; // it will never fit!

                BlockSize = newBlockSize;
                Pools.emplace_back(newBlockSize);
                pool = &Pools.back();
            }
            return pool->allocate(size, align);
        }

        // There is no deallocate -- by design !!
        void deallocate(void*) { }
    };


}
