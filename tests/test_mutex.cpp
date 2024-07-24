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
        auto& get_mutex() noexcept { return mutex; }
        auto& get_ref() noexcept { return value; }
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
            *simple = "Second value";
        });
        for (int i = 0; i < 10; ++i) { // 3 
            AssertEqual(*guard, "First value");
            sleep(1);
        }
        guard.unlock(); // 4
        t.join();
        AssertEqual(*simple, "Second value"); // 5
    }

    template<class T>
    class SafeVector : public rpp::synchronizable<SafeVector<T>>
    {
        std::vector<T> value;
        rpp::mutex mutex;
    public:
        auto& get_mutex() noexcept { return mutex; }
        auto& get_ref() noexcept { return value; }
    };

    TestCase(sync_guard_can_lock_vector)
    {
        SafeVector<int> vec;
        vec->push_back(1);
        vec->push_back(2);
        vec->push_back(3);

        AssertEqual(vec->size(), 3);
        AssertEqual(vec->at(0), 1);
        AssertEqual(vec->at(1), 2);
        AssertEqual(vec->at(2), 3);

        std::vector<int> iterated_values;
        for (auto& v : vec.guard())
            iterated_values.push_back(v);
        AssertEqual(iterated_values.size(), 3);
        AssertEqual(iterated_values[0], 1);
        AssertEqual(iterated_values[1], 2);
        AssertEqual(iterated_values[2], 3);

        AssertEqual(*vec, std::vector<int>({1,2,3}));
    };
};