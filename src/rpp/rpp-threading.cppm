// C++20 module interface unit for the rpp.threading headers, owned by tools/gen_module_exports.py.
// The headers stay in the global module fragment, so an importer and an includer share one entity.
module;

#include "mutex.h"
#include "condition_variable.h"
#include "semaphore.h"
#include "concurrent_queue.h"
#include "thread_pool.h"
#include "threads.h"
#include "task.h"
#include "future.h"
#include "future_types.h"
#include "event_loop.h"
#include "coroutines.h"
#include "atomic_shared_ptr.h"
#include "close_sync.h"

export module rpp.threading;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::mutex;
    using rpp::recursive_mutex;
    using rpp::unlock_guard;
    using rpp::spin_lock;
    using rpp::spin_lock_for;
    using rpp::SyncableType;
    using rpp::synchronize_guard;
    using rpp::synchronizable;
    using rpp::synchronized;
    using rpp::_cv_remaining_duration;
    using rpp::condition_variable;
    using rpp::parallel_task_detached;
    using rpp::semaphore;
    using rpp::semaphore_flag;
    using rpp::semaphore_once_flag;
    using rpp::atomic_test_and_set;
    using rpp::concurrent_queue;
    using rpp::seconds_t;
    using rpp::fseconds_t;
    using rpp::dseconds_t;
    using rpp::milliseconds_t;
    using rpp::duration_t;
    using rpp::action;
    using rpp::task_delegate;
    using rpp::pool_signal_handler;
    using rpp::pool_trace_provider;
    using rpp::wait_result;
    using rpp::pool_worker;
    using rpp::pool_task_state;
    using rpp::pool_task_handle;
    using rpp::thread_pool;
    using rpp::parallel_for;
    using rpp::parallel_foreach;
    using rpp::parallel_task;
    using rpp::yield;
    using rpp::task;
    using rpp::deferred;
    using rpp::cpromise;
    using rpp::async_task;
    using rpp::cfuture;
    using rpp::make_ready_future;
    using rpp::make_exceptional_future;
    using rpp::wait_all;
    using rpp::get_all;
    using rpp::run_tasks;
    using rpp::coro_handle;
    using rpp::suspend_never;
    using rpp::suspend_always;
    using rpp::IsFuture;
    using rpp::NotFuture;
    using rpp::IsFunction;
    using rpp::IsFunctionReturningFuture;
    using rpp::IsFunctionNotReturningFuture;
    using rpp::event_task;
    using rpp::event_loop;
    using rpp::functor_awaiter;
    using rpp::functor_awaiter_fut;
    using rpp::std_future_awaiter;
    using rpp::time_awaiter;
    using rpp::atomic_shared_ptr;
    using rpp::atomic_weak_ptr;
    using rpp::readonly_lock;
    using rpp::exclusive_lock;
    using rpp::close_sync;
#if !RPP_BARE_METAL
    using rpp::set_this_thread_name;
    using rpp::get_this_thread_name;
    using rpp::get_thread_name;
    using rpp::get_thread_id;
    using rpp::get_process_id;
    using rpp::num_physical_cores;
#endif
#if RPP_HAS_CRITICAL_SECTION_MUTEX
    using rpp::critical_section;
    using rpp::synchronized_critical;
#endif
}

export namespace rpp::coro_operators {
    using rpp::coro_operators::operator co_await;
}

export namespace rpp::detail {
    using rpp::detail::is_lvalue_ref;
    using rpp::detail::is_plain_lvalue_ref;
    using rpp::detail::BoolTestable;
    using rpp::detail::task_final_awaiter;
    using rpp::detail::take_result;
    using rpp::detail::task_promise;
    using rpp::detail::task_base;
}
// GENERATED EXPORTS END
