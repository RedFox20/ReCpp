#pragma once
#include <vector>

namespace rpp
{
    
    /**
     * Simplest type of memory pool.
     * Has a predetermined static size.
     * There is no deallocate!
     */
    class linear_static_pool
    {
        int Remaining;
        char* Buffer;
        char* Ptr;

    public:
        explicit linear_static_pool(int staticBlockSize)
            : Remaining{staticBlockSize}, Buffer{new char[staticBlockSize]}, Ptr{Buffer}
        {
        }
        ~linear_static_pool() noexcept
        {
            delete[] Buffer;
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

        void* allocate(int size)
        {
            if (Remaining < size)
                return nullptr;
            char* mem = Ptr;
            Ptr += size;
            Remaining -= size;
            return mem;
        }

        template<class T> T* allocate()
        {
            return reinterpret_cast<T*>(allocate(sizeof(T)));
        }

        template<class T> T* construct()
        {
            T* obj = reinterpret_cast<T*>(allocate(sizeof(T)));;
            return new (obj) T{};
        }

        template<class T, class... Args> T* construct(Args...args)
        {
            T* obj = reinterpret_cast<T*>(allocate(sizeof(T)));
            return new (obj) T{std::forward<Args>(args)...};
        }

        // There is no deallocate -- by design !!

        // Calls the destructor on the object
        template<class T> void destruct(T* obj)
        {
            obj->~T();
        }
    };


    /**
     * A memory pool that allocates by `bump the pointer`
     * and grows in large dynamic chunks.
     * Chunk growth can be controlled
     */
    class linear_dynamic_pool
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

        void* allocate(int size)
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
            return pool->allocate(size);
        }

        template<class T> T* allocate()
        {
            return reinterpret_cast<T*>(allocate(sizeof(T)));
        }

        template<class T> T* construct()
        {
            T* obj = reinterpret_cast<T*>(allocate(sizeof(T)));;
            return new (obj) T{};
        }

        template<class T, class... Args> T* construct(Args...args)
        {
            T* obj = reinterpret_cast<T*>(allocate(sizeof(T)));
            return new (obj) T{std::forward<Args>(args)...};
        }

        // There is no deallocate -- by design !!

        // Calls the destructor on the object
        template<class T> void destruct(T* obj)
        {
            obj->~T();
        }
    };


}
