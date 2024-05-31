#pragma once
#include <mutex> // lock_guard etc

namespace rpp
{
#if _MSC_VER
    #define USE_CUSTOM_WINDOWS_MUTEX 1
    #if USE_CUSTOM_WINDOWS_MUTEX
        struct mutex
        {
            struct { void* ctx; } mtx;

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
            void* native_handle() const noexcept { return (void*)&mtx.ctx; }
        };

        struct recursive_mutex
        {
            void* mtx;

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
}
