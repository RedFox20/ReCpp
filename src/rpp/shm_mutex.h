#pragma once
#if __linux__
#include <pthread.h>
#include <chrono>
#include <mutex>
#include <condition_variable>

namespace rpp {

    template<class Clock, class Duration>
    inline timespec to_timespec(const std::chrono::time_point<Clock, Duration>& tp)
    {
        using namespace std::chrono;
    
        // Get duration since the system_clock epoch, in nanoseconds
        auto d = tp.time_since_epoch();
        auto ns = duration_cast<nanoseconds>(d).count();
    
        timespec ts;
        ts.tv_sec  = ns / 1'000'000'000;
        ts.tv_nsec = ns % 1'000'000'000;
        return ts;
    }

    class shm_mutex
    {
        pthread_mutex_t handle;
    public:
        using native_handle_type = pthread_mutex_t*;
        
        shm_mutex() noexcept;
        ~shm_mutex() noexcept;

        // No copy/move
        shm_mutex(const shm_mutex&) = delete;
        shm_mutex& operator=(const shm_mutex&) = delete;
        shm_mutex(shm_mutex&&) = delete;
        shm_mutex& operator=(shm_mutex&&) = delete;

        void lock();
        bool try_lock();
        void unlock();

        native_handle_type native_handle() noexcept { return &handle; }
    };

    class shm_cond_var
    {
        shm_mutex cs;
        pthread_cond_t handle;

        using clock = std::chrono::high_resolution_clock;
        using duration = clock::duration;
        using time_point = clock::time_point;
    public:
        using native_handle_type = pthread_cond_t*;
        shm_cond_var();
        ~shm_cond_var() noexcept;

        // No copy/move
        shm_cond_var(const shm_cond_var&) = delete;
        shm_cond_var& operator=(const shm_cond_var&) = delete;
        shm_cond_var(shm_cond_var&&) = delete;
        shm_cond_var& operator=(shm_cond_var&&) = delete;

        void notify_one()
        {
            pthread_cond_signal(&handle);
        }
        void notify_all()
        {
            pthread_cond_broadcast(&handle);
        }

        void wait(std::unique_lock<shm_mutex>& lock)
        {
            cs.lock();
            lock.unlock();
            (void)pthread_cond_wait(&handle, cs.native_handle());
            cs.unlock();
            lock.lock();
        }

        std::cv_status wait_for(std::unique_lock<shm_mutex>& lock, const duration& rel_time)
        {
            auto abs_time = clock::now() + rel_time;
            return wait_until(lock, abs_time);
        }

        template<class Predicate>
        void wait(std::unique_lock<shm_mutex>& mtx, const Predicate& stop_waiting)
        {
            while (!stop_waiting())
                wait(mtx);
        }

        std::cv_status wait_until(std::unique_lock<shm_mutex>& lock, const time_point& abs_time)
        {
            cs.lock();
            lock.unlock();
            timespec abstime = to_timespec(abs_time);
            int err = pthread_cond_timedwait(&handle, cs.native_handle(), &abstime);
            std::cv_status status = (err == 0) ? std::cv_status::no_timeout : std::cv_status::timeout; // TODO: handle other errors
            cs.unlock();
            lock.lock();
            return status;
        }

        template<class Predicate>
        bool wait_until(std::unique_lock<shm_mutex>& lock, const time_point& abs_time,
                        const Predicate& stop_waiting)
        {
            while (!stop_waiting())
                if (wait_until(lock, abs_time) == std::cv_status::timeout)
                    return stop_waiting();
            return true;
        }

        template<class Predicate>
        bool wait_for(std::unique_lock<shm_mutex>& lock, const duration& rel_time,
                      const Predicate& stop_waiting)
        {
            auto abs_time = clock::now() + rel_time;
            while (!stop_waiting())
                if (wait_until(lock, abs_time) == std::cv_status::timeout)
                    return stop_waiting();
            return true;
        }

        native_handle_type native_handle() noexcept { return &handle; }
    };

} // namespace rpp

#endif // __linux__
