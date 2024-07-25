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
    template<typename T>
    concept SyncableType = requires(T t) {
        { t.get_mutex() };
        { t.get_ref() };
    };
    // Doesn't work for some reason :shrug:
    // #define RPP_SYNC_T SyncableType
    #define RPP_SYNC_T class
#else
    #define RPP_SYNC_T class
#endif

    template<RPP_SYNC_T SyncType>
    class synchronize_guard
    {
    public:
        using value_type = std::decay_t< decltype(std::declval<SyncType>().get_ref()) >;
        using mutex_type = std::decay_t< decltype(std::declval<SyncType>().get_mutex()) >;
    private:
        std::unique_lock<mutex_type> mtx_lock;
        SyncType* instance;
    public:
        explicit synchronize_guard(SyncType* s) noexcept
            : mtx_lock{rpp::spin_lock(s->get_mutex())}
            , instance{s}
        { }

        std::unique_lock<mutex_type>& lock() const noexcept { return mtx_lock; }
        bool owns_lock() const noexcept { return mtx_lock.owns_lock(); }
        void unlock() { mtx_lock.unlock(); }

        // NOTE: we don't allow all direct access operators, otherwise
        //       we're unable to insert instance->set() calls for classes that have it
        value_type* operator->() noexcept { return &instance->get_ref(); }
        // value_type& operator*() noexcept { return instance->get_ref(); }
        // value_type& get() noexcept { return instance->get_ref(); }
        // operator value_type&() noexcept { return instance->get_ref(); }

        const value_type* operator->() const noexcept { return &instance->get_ref(); }
        const value_type& operator*() const noexcept { return instance->get_ref(); }
        const value_type& get() const noexcept { return instance->get_ref(); }
        // operator const value_type&() const noexcept { return instance->get_ref(); }

        /**
         * @brief It's important that all write operations go through this operator,
         *        because we match SyncType::set() method if it exists.
         */
        template<class ValueType>
        void operator=(ValueType&& value) noexcept
        {
            // if class has a set() method, use it for assignments instead
            if constexpr (has_set_memb<SyncType, ValueType>)
                instance->set(std::forward<ValueType>(value));
            else
                instance->get_ref() = std::forward<ValueType>(value);
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
        begin() {
            return instance->get_ref().begin();
        }

        template<typename U = value_type>
        std::enable_if_t<is_iterable<U>, decltype(std::declval<U>().end())>
        end() {
            return instance->get_ref().end();
        }

        template<typename U = value_type>
        std::enable_if_t<is_iterable<U>, decltype(std::declval<const U>().begin())>
        begin() const {
            return static_cast<const U&>(instance->get_ref()).begin();
        }

        template<typename U = value_type>
        std::enable_if_t<is_iterable<U>, decltype(std::declval<const U>().end())>
        end() const {
            return static_cast<const U&>(instance->get_ref()).end(); 
        }
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
        const guard_type operator->() const noexcept { return guard_type{ static_cast<SyncType*>(const_cast<synchronizable*>(this)) }; }
        const guard_type operator*()  const noexcept { return guard_type{ static_cast<SyncType*>(const_cast<synchronizable*>(this)) }; }
        const guard_type guard()      const noexcept { return guard_type{ static_cast<SyncType*>(const_cast<synchronizable*>(this)) }; }
    };

    /**
     * @brief Provides a generic synchronized variable which can be used to wrap any type.
     * @tparam T The type of the synchronized variable, eg std::string or MyStateStruct
     * @code
     * rpp::synchronized<std::string> str { "Initial value" };
     * *str = "Thread safely set new value";
     * @endcode
     */
    template<class T>
    class synchronized : public synchronizable<synchronized<T>>
    {
    protected:
        T value {};
        rpp::recursive_mutex mutex {};
    public:

        synchronized() noexcept = default;
        synchronized(T&& value) noexcept : value{std::move(value)} {}
        synchronized(const T& value) noexcept : value{value} {}

        auto& get_mutex() noexcept { return mutex; }
        T& get_ref() noexcept { return value; }
    };
}
