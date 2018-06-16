#pragma once
/**
 * Read-Write synchronization of object destruction, Copyright (c) 2018, Jorma Rebane
 * Distributed under MIT Software License
 * 
 */
#include <shared_mutex>
#include <cassert>

namespace rpp
{
    using readonly_lock  = std::shared_lock<std::shared_mutex>;
    using exclusive_lock = std::unique_lock<std::shared_mutex>;


    /**
     * This helper attempts to ease the problem of async programming where the owning
     * class is destroyed while an async operation is in progress. Consider the following
     * example:
     * @code
     * class ImportantState
     * {
     *     vector<char> Data;
     * public:
     *    ~ImportantState()
     *    {
     *        // object is being destroyed
     *    }
     *    void SomeAsyncOperation()
     *    {
     *        parallel_task([this] {
     *            // is this->Data even valid??? what if THIS was already destroyed??
     *            Data.resize(64*1024);
     *        });
     *    }
     * };
     * @endcode
     * 
     * By adding close_sync and manually calling lock_for_close() in the destructor
     * we'll be able to delay the destruction until all async tasks are finished:
     * @code
     * class ImportantState
     * {
     *     close_sync CloseSync; // when using explicit lock, this should be first
     *     vector<char> Data;
     * public:
     *     ~ImportantState()
     *     {
     *         CloseSync.lock_for_close(); // blocks until async op is finished
     *     }
     *     void SomeAsyncOperation()
     *     {
     *        parallel_task([this] {
     *            try_lock_or_return(CloseSync);
     *            
     *            // this and this->Data will be alive until scope exit
     *            Data.resize(64*1024);
     *        });
     *     }
     * };
     * @endcode
     * 
     * Or the automatic version, where you always put your variables before CloseSync,
     * but does not provide perfect synchronization. It simply blocks before other
     * data members are destroyed:
     * @code
     * class ImportantState
     * {
     *     vector<char> Data;
     *     close_sync CloseSync; // this must be last!
     * public:
     *     void SomeAsyncOperation()
     *     {
     *        parallel_task([this] {
     *            try_lock_or_return(CloseSync);
     *
     *            // this and this->Data will be alive until scope exit
     *            Data.resize(64*1024);
     *        });
     *     }
     * };
     * @endcode
     */
    class close_sync
    {
        std::shared_mutex mut;
        bool explicitLock{ false };
        
        static constexpr short StillAlive = 0xB5C4;
        short aliveToken = StillAlive; // for validating if this object is still alive

    public:

        // this helper is NOCOPY/NOMOVE because such operations
        // will break any sane async code
        close_sync(close_sync&&) = delete;
        close_sync(const close_sync&) = delete;
        close_sync& operator=(close_sync&&) = delete;
        close_sync& operator=(const close_sync&) = delete;

        close_sync() = default;
        ~close_sync()
        {
            if (explicitLock) { // already explicitly locked for close
                aliveToken = 0;
                mut.unlock();
            }
            else { // no explicit locking used, so simply block until async tasks finish
                exclusive_lock exclusiveLock{ mut };
                aliveToken = 0;
            }
        }
        
        bool is_alive() const { return aliveToken == StillAlive; }
        
        /**
         * Acquires exclusive lock during destruction of the owning class.
         * It holds the lock until all fields declared after this are destroyed,
         * since fields are destructed in reverse order.
         * @note This should only be called in the destructor of the owning class.
         * @see acquire_exclusive_lock()
         */
        void lock_for_close()
        {
            assert(!explicitLock && "close_sync::lock_for_close called twice! This will deadlock.");
            explicitLock = true;
            mut.lock();
        }
        [[deprecated("try_lock has been replaced by try_readonly_lock")]]
        readonly_lock try_lock() noexcept
        {
            return { mut, std::try_to_lock };
        }
        readonly_lock try_readonly_lock() noexcept
        {
            if (!is_alive())
                return {};
            return { mut, std::try_to_lock };
        }
        exclusive_lock acquire_exclusive_lock() noexcept
        {
            return exclusive_lock{ mut };
        }
    };

    #ifndef RPP_CONCAT
    #  define RPP_CONCAT1(x,y) x##y
    #  define RPP_CONCAT(x,y) RPP_CONCAT1(x,y)
    #endif

    /**
     * Helper for rpp::close_sync. Usage:
     * @code
     * parallel_task([this] {
     *     try_lock_or_return(CloseSync);
     *     // this and this->Data will be alive until scope exit
     *     Data.resize(64*1024);
     * });
     * @endcode
     * 
     * Equivalent to:
     * @code
     * parallel_task([this] {
     *     auto lock { CloseSync.try_lock() };
     *     if (!lock) return;
     *     // this and this->Data will be alive until scope exit
     *     Data.resize(64*1024);
     * });
     * @endcode
     */
    #define try_lock_or_return(closeSync, ...) \
        auto RPP_CONCAT(lock_,__LINE__) { (closeSync).try_readonly_lock() }; \
        if (!RPP_CONCAT(lock_,__LINE__)) return ##__VA_ARGS__;

}
