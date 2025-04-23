#include "shm_mutex.h"
#include "debugging.h"
#include <condition_variable>

// TODO: throw instead of logging errors

namespace rpp {
    shm_mutex::shm_mutex() noexcept
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        if (int err = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED); err != 0)
        {
            LogError("shm_mutex: pthread_mutexattr_setpshared failed: error code %d", err);
            return;
        }
        pthread_mutex_init(&handle, &attr);
    }

    shm_mutex::~shm_mutex() noexcept
    {
        if (int err = pthread_mutex_destroy(&handle); err != 0)
        {
            LogError("shm_mutex: pthread_mutex_destroy failed: error code %d", err);
        }
    }

    void shm_mutex::lock()
    {
        if (int err = pthread_mutex_lock(&handle); err != 0)
        {
            LogError("shm_mutex: pthread_mutex_lock failed: error code %d", err);
        }
    }

    bool shm_mutex::try_lock()
    {
        int err = pthread_mutex_trylock(&handle);
        if (err != 0 && err != EBUSY)
        {
            LogError("shm_mutex: pthread_mutex_trylock failed: error code %d", err);
            return false;
        }
        return err == 0;
    }

    void shm_mutex::unlock()
    {
        if (int err = pthread_mutex_unlock(&handle); err != 0)
        {
            LogError("shm_mutex: pthread_mutex_unlock failed: error code %d", err);
        }
    }

    // -------------------------------------------------------

    shm_cond_var::shm_cond_var()
    {
        pthread_condattr_t attr;
        pthread_condattr_init(&attr);
        if (int err = pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED); err != 0)
        {
            LogError("shm_mutex: pthread_mutexattr_setpshared failed: error code %d", err);
            return;
        }
        pthread_cond_init(&handle, &attr);
    }

    shm_cond_var::~shm_cond_var() noexcept
    {
        if (int err = pthread_cond_destroy(&handle); err != 0)
        {
            LogError("shm_mutex: pthread_mutex_destroy failed: error code %d", err);
        }
    }

} // namespace rpp
