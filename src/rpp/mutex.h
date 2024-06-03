#pragma once
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
}
