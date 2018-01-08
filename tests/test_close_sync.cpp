#include <rpp/close_sync.h>
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
        string data = "xxxxyyyyzzzzaaaabbbbcccc";

        ~ImportantState()
        {
            CloseSync.lock_for_close();
            AssertThat(data, "aaaabbbbcccc");
            data = "???????????";
        }

        void SomeAsyncOperation()
        {
            parallel_task([this]
            {
                try_lock_or_return(CloseSync);
                ::sleep_for(30ms);
                AssertThat(data, "xxxxyyyyzzzzaaaabbbbcccc");
                data = "aaaabbbbcccc";
            });
            ::sleep_for(1ms);
        }
    };

    TestCase(basic_close_prevention)
    {
        {
            ImportantState is;
            is.SomeAsyncOperation();
        }
    }

} Impl;
