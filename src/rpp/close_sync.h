#pragma once
/**
 * Read-Write synchronization of object destruction, Copyright (c) 2018, Jorma Rebane
 * Distributed under MIT Software License
 * 
 */
#include "mutex.h" // unique_lock
#include "debugging.h"
#include <shared_mutex> // shared_lock

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
     * we'll be able to delay the destruction until all async Tasks are finished:
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
        bool locked_for_close = false;
        
        static constexpr unsigned short STILL_ALIVE = (unsigned short)0xB5C4;
        unsigned short alive_token = STILL_ALIVE; // for validating if this object is still alive

    public:
        close_sync() noexcept = default;
        ~close_sync() noexcept
        {
            if (locked_for_close) { // already explicitly locked for close
                alive_token = 0;
                mut.unlock();
            }
            else { // no explicit locking used, so simply block until async Tasks finish
                exclusive_lock exclusive_lock { mut };
                alive_token = 0;
            }
        }
        
        // NOCOPY/NOMOVE because such operations will break async code
        close_sync(close_sync&&) = delete;
        close_sync(const close_sync&) = delete;
        close_sync& operator=(close_sync&&) = delete;
        close_sync& operator=(const close_sync&) = delete;

        /** @returns TRUE if CloseSync is still alive and destructor hasn't deleted it */
        bool is_alive() const noexcept { return alive_token == STILL_ALIVE; }

        /** @returns TRUE if lock_for_close() has been called */
        bool is_closing() const noexcept { return locked_for_close; }

        /** @returns TRUE if lock_for_close() has been called or already destroyed */
        bool is_dead_or_closing() const noexcept { return locked_for_close || !is_alive(); }

        /**
         * Acquires exclusive lock during destruction of the owning class.
         * It holds the lock until all fields declared after this are destroyed,
         * since fields are destructed in reverse order.
         * 
         * @note This should only be called in the destructor of the owning class.
         * @see acquire_exclusive_lock()
         */
        void lock_for_close() noexcept
        {
            if (locked_for_close)
            {
                LogError("close_sync::lock_for_close called twice! This is a bug.");
                return; // already locked
            }
            locked_for_close = true;
            mut.lock();
        }

        /**
         * @brief Attempts to acquire a Read-Only (shared) lock
         * The lock needs to be checked.
         */
        [[nodiscard]] readonly_lock try_readonly_lock() noexcept
        {
            if (!is_alive())
                return {};
            return readonly_lock{ mut, std::try_to_lock };
        }

        [[nodiscard]] exclusive_lock acquire_exclusive_lock() noexcept
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
        if (!RPP_CONCAT(lock_,__LINE__)) return __VA_ARGS__;

}
