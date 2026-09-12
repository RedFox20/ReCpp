// The std atomic and locking names a public ReCpp signature writes. `rpp.std` re-exports it.
module;

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>

export module rpp.std.threading;

export namespace std {
    using std::atomic;
    using std::atomic_int;
    using std::atomic_bool;
    using std::atomic_int64_t;
    using std::memory_order;
    using std::memory_order_relaxed;
    using std::memory_order_acquire;
    using std::memory_order_release;
    using std::memory_order_acq_rel;
    using std::memory_order_seq_cst;

    // rpp::condition_variable::wait_for takes a std::unique_lock and returns a
    // std::cv_status, and close_sync.h names std::shared_mutex in a public alias
    using std::mutex;
    using std::unique_lock;
    using std::lock_guard;
    using std::shared_mutex;
    using std::shared_lock;
    using std::condition_variable;
    using std::cv_status;
}

// <future> stays out, see BUGS.md B20. std::future, std::promise and std::future_status go
// with it, and rpp::cfuture::await_ready() answers without the header.
