#pragma once
#include "config.h"
#include "type_traits.h"
#include <mutex> // lock_guard etc
#include <thread> // this_thread::yield

namespace rpp
{
#if _MSC_VER
    #define USE_CUSTOM_WINDOWS_MUTEX 1
    #if USE_CUSTOM_WINDOWS_MUTEX
        class mutex
        {
            struct { void* ctx; } mtx;
        public:
            mutex() noexcept;
            ~mutex() noexcept;

            mutex(mutex&&) = delete;
            mutex& operator=(mutex&&) = delete;

            mutex(const mutex&) = delete;
            mutex& operator=(const mutex&) = delete;

            bool try_lock() noexcept;
            void lock();
            void unlock() noexcept;

            // this mutex is always valid and not copyable
            void* native_handle() const noexcept { return (void*)&mtx; }
        };

        class recursive_mutex
        {
            void* mtx;
        public:
            recursive_mutex() noexcept;
            ~recursive_mutex() noexcept;

            recursive_mutex(recursive_mutex&& m) noexcept : mtx{m.mtx}
            {
                m.mtx = nullptr;
            }
            recursive_mutex& operator=(recursive_mutex&& m) noexcept
            {
                std::swap(mtx, m.mtx);
                return *this;
            }

            bool try_lock() noexcept;
            void lock();
            void unlock() noexcept;

            void* native_handle() const noexcept { return mtx; }
        };
    #else // USE_CUSTOM_WINDOWS_MUTEX
        using mutex = std::mutex;
        using recursive_mutex = std::recursive_mutex;
    #endif // USE_CUSTOM_WINDOWS_MUTEX
#else
    using mutex = std::mutex;
    using recursive_mutex = std::recursive_mutex;
#endif

    /**
     * @brief Unlock Guard: RAII unlocks and then relocks on scope exit.
     */
    template<class Mutex>
    struct unlock_guard final
    {
        std::unique_lock<Mutex>& mtx;
        explicit unlock_guard(std::unique_lock<Mutex>& mtx) : mtx{mtx} {
            mtx.unlock();
        }
        ~unlock_guard() noexcept /* terminates */ { // MSVC++ defined
            // relock mutex or terminate()
            // condition_variable_any wait functions are required to terminate if
            // the mutex cannot be relocked;
            // we slam into noexcept here for easier user debugging.
            mtx.lock();
        }
    };

    /**
     * @brief Performs a few spins before locking and suspending the thread.
     * This will massive improve locking performance in high-contention scenarios.
     */
    template<class Mutex>
    inline std::unique_lock<Mutex> spin_lock(Mutex& m) noexcept
    {
        if (!m.try_lock()) // spin until we can lock the mutex
        {
            for (int i = 0; i < 10; ++i)
            {
                std::this_thread::yield(); // yielding here will improve perf massively
                if (m.try_lock())
                    return std::unique_lock<Mutex>{m, std::adopt_lock};
            }
            // suspend until we can lock the mutex
            try {
                m.lock(); // may throw if deadlock
            } catch (...) {
                // if we failed to lock, this is most likely a deadlock or the mutex is destroyed
                // simply give up and return a deferred lock
                return std::unique_lock<Mutex>{m, std::defer_lock};
            }
        }
        return std::unique_lock<Mutex>{m, std::adopt_lock};
    }

#if RPP_HAS_CXX20
    template<typename S>
    concept SyncableType = requires(S syncable) {
        syncable.get_mutex(); // example:  rpp::mutex& get_mutex() noexcept { ... }
        syncable.get_ref();   // example:  std::string& get_ref() noexcept { ... }
    };
    template<typename M>
    concept BasicLockable = requires(M mutex) {
        mutex.lock();
        mutex.unlock();
    };
    // #define RPP_SYNC_T SyncableType
    // #define RPP_SYNC_MUTEX_T BasicLockable
    #define RPP_SYNC_T class
    #define RPP_SYNC_MUTEX_T class
#else
    #define RPP_SYNC_T class
    #define RPP_SYNC_MUTEX_T class
#endif

    template<RPP_SYNC_T SyncType>
    class synchronize_guard
    {
    public:
        using value_type = std::decay_t< decltype(std::declval<SyncType>().get_ref()) >;
        using mutex_type = std::decay_t< decltype(std::declval<SyncType>().get_mutex()) >;
    private:
        std::unique_lock<mutex_type> lock;
        SyncType* instance;
    public:
        explicit synchronize_guard(SyncType* s) noexcept
            : lock{rpp::spin_lock(s->get_mutex())}
            , instance{s}
        { }

        void unlock() { lock.unlock(); }

        value_type* operator->() noexcept { return &instance->get_ref(); }
        value_type& operator*() noexcept { return instance->get_ref(); }
        value_type& get() noexcept { return instance->get_ref(); }
        operator value_type&() noexcept { return instance->get_ref(); }

        const value_type* operator->() const noexcept { return &instance->get_ref(); }
        const value_type& operator*() const noexcept { return instance->get_ref(); }
        const value_type& get() const noexcept { return instance->get_ref(); }
        operator const value_type&() const noexcept { return instance->get_ref(); }

        void operator=(const value_type& value) noexcept
        {
            instance->get_ref() = value;
        }
        void operator=(value_type&& value) noexcept
        {
            instance->get_ref() = std::move(value);
        }

        template<class U> FINLINE bool operator==(const U& other) const noexcept
        {
            return instance->get_ref() == other;
        }
        template<class U> FINLINE bool operator!=(const U& other) const noexcept
        {
            return instance->get_ref() != other;
        }

        template<typename U = value_type>
        std::enable_if_t<is_iterable<U>, decltype(std::declval<U>().begin())>
        begin() { return get().begin(); }

        template<typename U = value_type>
        std::enable_if_t<is_iterable<U>, decltype(std::declval<U>().end())>
        end() { return get().end(); }

        template<typename U = value_type>
        std::enable_if_t<is_iterable<U>, decltype(std::declval<const U>().begin())>
        begin() const { return get().begin(); }

        template<typename U = value_type>
        std::enable_if_t<is_iterable<U>, decltype(std::declval<const U>().end())>
        end() const { return get().end(); }
    };

    /**
     * @brief Provides a generalized synchronization proxy which any class can inherit from.
     * @tparam SyncType The type of the class inheriting from synchronizable
     * @code
     * class Example : public rpp::synchronizable<Example>
     * {
     *     std::string value;
     *     rpp::mutex mutex;
     * public:
     *     rpp::mutex& get_mutex() noexcept { return mutex; }
     *     std::string& get_ref() noexcept { return value; }
     * };
     * @endcode
     */
    template<RPP_SYNC_T SyncType>
    class synchronizable
    {
    public:
        synchronizable() noexcept = default;

        // NOMOVE / NOCOPY
        synchronizable(synchronizable&&) = delete;
        synchronizable(const synchronizable&) = delete;
        synchronizable& operator=(synchronizable&&) = delete;
        synchronizable& operator=(const synchronizable&) = delete;

        using guard_type = rpp::synchronize_guard<SyncType>;

        guard_type operator->() noexcept { return guard_type{ static_cast<SyncType*>(this) }; }
        guard_type operator*()  noexcept { return guard_type{ static_cast<SyncType*>(this) }; }
        guard_type guard()      noexcept { return guard_type{ static_cast<SyncType*>(this) }; }
        const guard_type operator->() const noexcept { return guard_type{ static_cast<SyncType*>(this) }; }
        const guard_type operator*()  const noexcept { return guard_type{ static_cast<SyncType*>(this) }; }
        const guard_type guard()      const noexcept { return guard_type{ static_cast<SyncType*>(this) }; }
    };
}
