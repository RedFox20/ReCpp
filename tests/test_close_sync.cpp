#include <rpp/close_sync.h>
#include <rpp/semaphore.h>
#include <rpp/thread_pool.h>
#include <rpp/tests.h>
using namespace rpp;
using namespace std::chrono_literals;
using namespace std::this_thread;

TestImpl(test_close_sync)
{
    TestInit(test_close_sync)
    {
    }

    struct ImportantState
    {
        close_sync CloseSync;
        rpp::semaphore_once_flag Locked;
        std::string data = "xxxxyyyyzzzzaaaabbbbcccc";

        ~ImportantState() noexcept(false)
        {
            CloseSync.lock_for_close();
            if (data != "aaaabbbbcccc")
                throw std::runtime_error("~ImportantState: data != \"aaaabbbbcccc\"");
            data = "???????????";
        }

        void SomeAsyncOperation()
        {
            parallel_task([this]
            {
                try_lock_or_return(CloseSync);
                Locked.notify(); // the worker holds the close_sync from here
                ::sleep_for(30ms);
                //AssertThat(data, "xxxxyyyyzzzzaaaabbbbcccc");
                if (data != "xxxxyyyyzzzzaaaabbbbcccc")
                    throw std::runtime_error("SomeAsyncOperation: data != \"xxxxyyyyzzzzaaaabbbbcccc\"");
                data = "aaaabbbbcccc";
            });
            Locked.wait(); // the destructor must not reach lock_for_close() before the worker holds it
        }
    };

    TestCase(basic_close_prevention)
    {
        {
            ImportantState is;
            is.SomeAsyncOperation();
        }
    }

};
