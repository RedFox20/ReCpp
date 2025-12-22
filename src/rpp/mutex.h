#pragma once
#include "config.h"
#include "type_traits.h"
#include "timer.h" // rpp:Timer
#include <mutex> // lock_guard etc

#if !RPP_BARE_METAL
    #include <thread> // this_thread::yield
#endif

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

#elif RPP_FREERTOS
    class critical_section
    {
    public:
        critical_section() noexcept = default;

        // No copy/move
        critical_section(const critical_section&) = delete;
        critical_section& operator=(const critical_section&) = delete;
        critical_section(critical_section&& other) = delete;
        critical_section& operator=(critical_section&& other) = delete;

        bool try_lock() noexcept;
        void lock() noexcept;
        void unlock() noexcept;

        void* native_handle() const noexcept { return this; }
    };

    class mutex
    {
        void* ctx;
    public:
        mutex() noexcept;
        ~mutex() noexcept;

        // No copy/move
        mutex(const mutex&) = delete;
        mutex& operator=(const mutex&) = delete;
        mutex(mutex&& other) = delete;
        mutex& operator=(mutex&& other) = delete;

        bool try_lock() noexcept;
        void lock() noexcept;
        void unlock() noexcept;

        void* native_handle() const noexcept { return ctx; }
    };

    class recursive_mutex
    {
        void* ctx;

    public:
        recursive_mutex() noexcept;
        ~recursive_mutex() noexcept;

        // No copy/move
        recursive_mutex(const recursive_mutex&) = delete;
        recursive_mutex& operator=(const recursive_mutex&) = delete;
        recursive_mutex(recursive_mutex&& other) = delete;
        recursive_mutex& operator=(recursive_mutex&& other) = delete;
        
        bool try_lock() noexcept;
        void lock() noexcept;
        void unlock() noexcept;

        void* native_handle() const noexcept { return ctx; }
    };

    #define RPP_HAS_CRITICAL_SECTION_MUTEX 1
#elif RPP_CORTEX_M_ARCH
    /**
     * @brief Disables interrupts, preventing context switches.
     * Locking and unlocking always succeeds.
     */
    class critical_section
    {
        struct {
            uint32_t primask = 0;
            uint32_t locked = 0;
        } mtx;
    public:
        critical_section() noexcept = default;

        // No copy/move
        critical_section(const critical_section&) = delete;
        critical_section& operator=(const critical_section&) = delete;
        critical_section(critical_section&& other) = delete;
        critical_section& operator=(critical_section&& other) = delete;
        
        bool try_lock() noexcept; // Always succeed, so it's an alias for lock();
        void lock() noexcept;
        void unlock() noexcept;

        void* native_handle() const noexcept { return (void*) &mtx; }
    };

    using mutex = critical_section;
    using recursive_mutex = critical_section;

#define RPP_HAS_CRITICAL_SECTION_MUTEX 1
#else
    using mutex = std::mutex;
    using recursive_mutex = std::recursive_mutex;
#endif

#ifndef RPP_HAS_CRITICAL_SECTION_MUTEX
    #define RPP_HAS_CRITICAL_SECTION_MUTEX 0
#endif

        /**
         * @brief Unlock Guard: RAII unlocks and then relocks on scope exit.
         */
        template <class Mutex>
        struct unlock_guard final
        {
            std::unique_lock<Mutex>& mtx;
            explicit unlock_guard(std::unique_lock<Mutex>& mtx) : mtx{mtx}
            {
                mtx.unlock();
            }
            ~unlock_guard() noexcept /* terminates */
            {                        // MSVC++ defined
                // relock mutex or terminate()
                // condition_variable_any wait functions are required to terminate if
                // the mutex cannot be relocked;
                // we slam into noexcept here for easier user debugging.
                mtx.lock();
            }
    };

    /**
     * @brief Yields the current thread, allowing other threads to run.
     */
    FINLINE void yield() noexcept
    {
        #if RPP_FREERTOS
            if (xPortIsInsideInterrupt())
                return; // cannot yield from ISR
            taskYIELD();
        #elif RPP_STM32_HAL
            __NOP();
        #else
            std::this_thread::yield();
        #endif
    }

    /**
     * @brief Performs a few spins before locking and suspending the thread.
     * This will massive improve locking performance in high-contention scenarios.
     * 
     * @returns Owned Lock if successful, Deferred Lock if lock was not acquired (suspended lock threw exception)
     */
    template<class Mutex>
    inline std::unique_lock<Mutex> spin_lock(Mutex& m) noexcept
    {
        if (!m.try_lock()) // spin until we can lock the mutex
        {
            for (int i = 0; i < 10; ++i)
            {
                 // yielding here will improve perf massively
                #if RPP_FREERTOS
                    taskYIELD();
                #elif RPP_STM32_HAL
                    __NOP();
                #else
                    std::this_thread::yield();
                #endif
                if (m.try_lock())
                    return std::unique_lock<Mutex>{m, std::adopt_lock};
            }
            // suspend until we can lock the mutex
            #if RPP_BARE_METAL
                // No exceptions in bare-metal mode, just lock
                m.lock();
            #else
                try {
                    m.lock(); // may throw if deadlock
                } catch (...) {
                    // if we failed to lock, this is most likely a deadlock or the mutex is destroyed
                    // simply give up and return a deferred lock
                    return std::unique_lock<Mutex>{m, std::defer_lock};
                }
            #endif
        }
        return std::unique_lock<Mutex>{m, std::adopt_lock};
    }

    /**
     * @brief Tries to spin-lock the mutex until the given timeout is reached.
     * This works for regular non-timed mutexes that don't support try_lock_for().
     * 
     * @returns Owned Lock if successful, Deferred Lock if lock was not acquired (timeout)
     */
    template<class Mutex>
    inline std::unique_lock<Mutex> spin_lock_for(Mutex& m, rpp::Duration timeout) noexcept
    {
        if (!m.try_lock()) // spin until we can lock the mutex
        {
            if (timeout <= rpp::Duration::zero())
                return std::unique_lock<Mutex>{m, std::defer_lock}; // no timeout, just return deferred lock

            rpp::TimePoint start = rpp::TimePoint::now();
            do {
                rpp::sleep_us(100); // yielding here will improve perf massively
                if (m.try_lock()) return std::unique_lock<Mutex>{m, std::adopt_lock};
            }
            while ((rpp::TimePoint::now() - start) < timeout);

            return std::unique_lock<Mutex>{m, std::defer_lock}; // no timeout, just return deferred lock
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

        std::unique_lock<mutex_type>& get_lock() noexcept { return mtx_lock; }
        bool owns_lock() const noexcept { return mtx_lock.owns_lock(); }
        void lock() { mtx_lock.lock(); }
        void unlock() { mtx_lock.unlock(); }

        // NOTE: Do not allow overwriting accessors, because all writes need to go 
        //       through synchronize_guard::operator=() which detects SyncType::set() method.
        //       However existing value can be modified via operator->()
        value_type* operator->() noexcept { return &instance->get_ref(); }

        // For const refs, all read accessors are allowed
        // WARNING: do not const_cast these, it will cause undefined behavior
        const value_type* operator->() const noexcept { return &instance->get_ref(); }
        const value_type& operator*() const noexcept { return instance->get_ref(); }
        const value_type& get() const noexcept { return instance->get_ref(); }
        operator const value_type&() const noexcept { return instance->get_ref(); }

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

        template<class U = value_type, class V,
                 std::enable_if_t<rpp::is_iterable<U>, int> = 0>
        FINLINE void operator=(std::initializer_list<V> value)
        {
            this->operator=(U{value});
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

        synchronized() noexcept(noexcept(T{})) = default;
        synchronized(T&& value) noexcept : value{std::move(value)} {}
        synchronized(const T& value) noexcept : value{value} {}

        auto& get_mutex() noexcept { return mutex; }
        T& get_ref() noexcept { return value; }
    };

#if RPP_HAS_CRITICAL_SECTION_MUTEX
    /**
     * @brief Same as rpp::synchronized but uses rpp::critical_section for mutexing.
     */
    template<class T>
    class synchronized_critical : public synchronizable<synchronized_critical<T>>
    {
    protected:
        T value {};
        rpp::critical_section mutex {};
    public:

        synchronized_critical() noexcept(noexcept(T{})) = default;
        synchronized_critical(T&& value) noexcept : value{std::move(value)} {}
        synchronized_critical(const T& value) noexcept : value{value} {}

        auto& get_mutex() noexcept { return mutex; }
        T& get_ref() noexcept { return value; }
    }
#endif
}
