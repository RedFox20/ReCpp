#pragma once
/**
 * Portable atomic shared_ptr wrapper, Copyright (c) 2024-2026, Jorma Rebane
 * Distributed under MIT Software License
 *
 * std::atomic<std::shared_ptr<T>> (C++20, P0718R2) is supported by
 * libstdc++ (GCC 12+) and MSVC (19.28+), but NOT by libc++ (Clang).
 * This header provides a portable wrapper:
 *   - Native alias when __cpp_lib_atomic_shared_ptr is defined
 *   - Spinlock-based fallback otherwise
 */
#include "config.h"
#include "mutex.h"
#include <memory>
#include <atomic>

namespace rpp
{
    //////////////////////////////////////////////////////////////////////////////////////////

#ifndef RPP_USE_STD_ATOMIC_SHARED_PTR
#  ifdef __cpp_lib_atomic_shared_ptr
#    define RPP_USE_STD_ATOMIC_SHARED_PTR 1
#  else
#    define RPP_USE_STD_ATOMIC_SHARED_PTR 0
#  endif
#endif

#if RPP_USE_STD_ATOMIC_SHARED_PTR

    /**
     * Native std::atomic<shared_ptr<T>> available (GCC 12+, MSVC 19.28+)
     */
    template<class T>
    using atomic_shared_ptr = std::atomic<std::shared_ptr<T>>;

    template<class T>
    using atomic_weak_ptr = std::atomic<std::weak_ptr<T>>;

#else

    /**
     * Fallback: lock based atomic shared_ptr for platforms without
     * native std::atomic<shared_ptr<T>> (notably libc++/Clang).
     *
     * Critical sections are sub-microsecond (shared_ptr copy/move only),
     * making a spinlock appropriate.
     */
    template<class T>
    class atomic_shared_ptr
    {
        std::shared_ptr<T> ptr;
        // lesson: mutex is faster than std::spin_lock in this case
        mutable rpp::mutex mtx {};

    public:
        atomic_shared_ptr() noexcept = default;

        atomic_shared_ptr(std::shared_ptr<T> desired) noexcept
            : ptr{std::move(desired)} {}

        ~atomic_shared_ptr() noexcept = default;

        // Non-copyable, non-movable (matches std::atomic semantics)
        atomic_shared_ptr(const atomic_shared_ptr&) = delete;
        atomic_shared_ptr& operator=(const atomic_shared_ptr&) = delete;

        bool is_lock_free() const noexcept { return false; }

        atomic_shared_ptr& operator=(std::nullptr_t) noexcept
        {
            mtx.lock();
            this->ptr.reset();
            mtx.unlock();
            return *this;
        }

        std::shared_ptr<T> load(std::memory_order /*order*/ = std::memory_order_seq_cst) const noexcept
        {
            mtx.lock();
            std::shared_ptr<T> result = this->ptr;
            mtx.unlock();
            return result;
        }

        void store(std::shared_ptr<T> desired,
                   std::memory_order /*order*/ = std::memory_order_seq_cst) noexcept
        {
            mtx.lock();
            this->ptr = std::move(desired);
            mtx.unlock();
        }

        std::shared_ptr<T> exchange(std::shared_ptr<T> desired,
                                    std::memory_order /*order*/ = std::memory_order_seq_cst) noexcept
        {
            mtx.lock();
            std::shared_ptr<T> old = std::move(this->ptr);
            this->ptr = std::move(desired);
            mtx.unlock();
            return old;
        }
    };

    /**
     * Fallback: lock based atomic weak_ptr for platforms without
     * native std::atomic<weak_ptr<T>> (notably libc++/Clang).
     */
    template<class T>
    class atomic_weak_ptr
    {
        std::weak_ptr<T> ptr;
        // lesson: mutex is faster than std::spin_lock in this case
        mutable rpp::mutex mtx {};

    public:
        atomic_weak_ptr() noexcept = default;

        atomic_weak_ptr(std::weak_ptr<T> desired) noexcept
            : ptr{std::move(desired)} {}

        ~atomic_weak_ptr() noexcept = default;

        // Non-copyable, non-movable (matches std::atomic semantics)
        atomic_weak_ptr(const atomic_weak_ptr&) = delete;
        atomic_weak_ptr& operator=(const atomic_weak_ptr&) = delete;

        bool is_lock_free() const noexcept { return false; }

        atomic_weak_ptr& operator=(std::nullptr_t) noexcept
        {
            mtx.lock();
            this->ptr.reset();
            mtx.unlock();
            return *this;
        }

        std::weak_ptr<T> load(std::memory_order /*order*/ = std::memory_order_seq_cst) const noexcept
        {
            mtx.lock();
            std::weak_ptr<T> result = this->ptr;
            mtx.unlock();
            return result;
        }

        void store(std::weak_ptr<T> desired,
                   std::memory_order /*order*/ = std::memory_order_seq_cst) noexcept
        {
            mtx.lock();
            this->ptr = std::move(desired);
            mtx.unlock();
        }

        std::weak_ptr<T> exchange(std::weak_ptr<T> desired,
                                  std::memory_order /*order*/ = std::memory_order_seq_cst) noexcept
        {
            mtx.lock();
            std::weak_ptr<T> old = std::move(this->ptr);
            this->ptr = std::move(desired);
            mtx.unlock();
            return old;
        }
    };

#endif

    //////////////////////////////////////////////////////////////////////////////////////////

} // namespace rpp
