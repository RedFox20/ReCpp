#include <rpp/tests.h>
#include <rpp/mutex.h>

TestImpl(test_mutex)
{
    TestInit(test_mutex)
    {
    }

    class SimpleValue : public rpp::synchronizable<SimpleValue>
    {
        std::string value;
        rpp::mutex mutex;
    public:
        rpp::mutex& get_mutex() noexcept { return mutex; }
        std::string& get_ref() noexcept { return value; }
    };

    TestCase(sync_guard_can_lock_simple_value)
    {
        SimpleValue simple;

        *simple = "Testing operator*";
        AssertEqual(*simple, "Testing operator*");
        simple->assign("Testing operator->");
        AssertEqual(*simple, "Testing operator->");
        (*simple).get() = "Testing get()";
        AssertEqual((*simple).get(), "Testing get()");

        // 1. lock and set First value
        // 2. spawn a thread which sets Second value
        // 3. check in a loop that it is not modified
        // 4. unlock the value and join the thread
        // 5. ensure it has the Second value
        auto guard = simple.guard(); // 1
        *guard = "First value";
        auto t = std::thread([&] { // 2
            *guard = "Second value";
        });
        for (int i = 0; i < 10; ++i) { // 3 
            AssertEqual(*guard, "First value");
            sleep(1);
        }
        guard.unlock(); // 4
        t.join();
        AssertEqual(*simple, "Second value"); // 5
    }

};