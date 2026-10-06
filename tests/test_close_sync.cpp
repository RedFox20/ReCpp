#include <rpp/close_sync.h>
#include <rpp/semaphore.h>
#include <rpp/thread_pool.h>
#include <rpp/timepoint.h> // rpp::sleep_ms
#include <rpp/tests.h>
using namespace rpp;

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
                // holds the close_sync, so the destructor must wait in lock_for_close()
                rpp::sleep_ms(30);
                //AssertThat(data, "xxxxyyyyzzzzaaaabbbbcccc");
                if (data != "xxxxyyyyzzzzaaaabbbbcccc")
                    throw std::runtime_error("SomeAsyncOperation: data != \"xxxxyyyyzzzzaaaabbbbcccc\"");
                data = "aaaabbbbcccc";
            });
            // the worker releases this from inside the close_sync, so a timeout means it never ran
            if (Locked.wait(rpp::seconds(1)) == rpp::semaphore::timeout)
                throw std::runtime_error("SomeAsyncOperation: the worker never took the close_sync");
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
