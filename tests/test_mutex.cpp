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
        auto& get_mutex() { return mutex; }
        auto& get_ref() { return value; }
    };

    TestCase(sync_guard_can_lock_simple_value)
    {
        SimpleValue simple;

        *simple = "Testing operator*";
        AssertEqual(*simple, "Testing operator*");

        simple->assign("Testing operator->");
        AssertEqual(*simple, "Testing operator->");

        *simple = "Testing get()";
        AssertEqual((*simple).get(), "Testing get()");

        // 1. lock and set First value
        // 2. spawn a thread which sets Second value
        // 3. check in a loop that it is not modified
        // 4. unlock the value and join the thread
        // 5. ensure it has the Second value
        auto guard = simple.guard(); // 1
        guard = "First value";
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

    template<class T, class Mutex = rpp::mutex>
    class SafeVector : public rpp::synchronizable<SafeVector<T>>
    {
    public:
        std::vector<T> value;
        Mutex mutex;
        auto& get_mutex() noexcept { return mutex; }
        auto& get_ref() noexcept { return value; }
    };

    TestCase(sync_guard_can_lock_vector)
    {
        SafeVector<int> vec;
        *vec = {1,2,3};

        AssertEqual(vec->size(), 3);
        AssertEqual(vec->at(0), 1);
        AssertEqual(vec->at(1), 2);
        AssertEqual(vec->at(2), 3);
        AssertEqual(*vec, std::vector<int>({1,2,3}));

        std::vector<int> iterated_values;
        for (auto& v : *vec)
            iterated_values.push_back(v);
        AssertEqual(iterated_values.size(), 3u);
        AssertEqual(iterated_values[0], 1);
        AssertEqual(iterated_values[1], 2);
        AssertEqual(iterated_values[2], 3);

        // the guard will prevent modification in background thread
        auto guard = vec.guard();
        auto t = std::thread([&]
        {
            vec->push_back(4);
            vec->push_back(5);
            vec->push_back(6);
        });

        for (int i = 0; i < 10; ++i)
        {
            AssertEqual(*guard, std::vector<int>({1,2,3}));
            sleep(1);
        }

        guard.unlock();
        t.join();

        AssertEqual(*vec, std::vector<int>({1,2,3,4,5,6}));
    };

    TestCase(sync_guard_holds_lock_during_iteration)
    {
        SafeVector<int, rpp::recursive_mutex> vec;
        *vec = {1,2,3};

        // try to acquire lock and modify the vector
        auto task = std::thread([&]
        {
            sleep(5);
            auto guard = vec.guard();
            guard->push_back(4);
            guard->push_back(5);
            guard->push_back(6);
        });

        // immediately acquire lock and slowly iterate over the vector
        for (auto& v : *vec)
        {
            sleep(10);
            AssertTrue(v == 1 || v == 2 || v == 3);
            AssertEqual(vec->size(), 3u);
        }

        // now lock should be released and vec will be modified
        task.join();
        AssertEqual(*vec, std::vector<int>({1,2,3,4,5,6}));
    }

    class WithSetMethod : public rpp::synchronizable<WithSetMethod>
    {
    public:
        std::string value;
        rpp::mutex mutex;
        bool called_set = false;

        WithSetMethod(std::string&& value) : value(std::move(value)) {}

        auto& get_mutex() { return mutex; }
        auto& get_ref() { return value; }

        template<class U> void set(U&& new_value) { 
            value = std::forward<U>(new_value);
            called_set = true;
        }
    };

    TestCase(sync_guard_uses_set_method_on_synced_type)
    {
        WithSetMethod var { "Initial value" };
        AssertEqual(*var, "Initial value");
        AssertFalse(var.called_set);

        *var = "Testing operator set()";
        AssertEqual(*var, "Testing operator set()");
        AssertTrue(var.called_set);
        var.called_set = false;
    }

    TestCase(sync_guard_locks_during_function_call)
    {
        WithSetMethod var { "Initial value" };

        auto t = std::thread([&]{
            sleep(5);
            *var = "Setting new value";
        });

        // will hold the lock for a long time
        auto fun = [&](const std::string& s)
        {
            sleep(10);
            AssertEqual(s, "Initial value");
        };
        fun(*var);

        t.join();
        AssertEqual(*var, "Setting new value");
    }

    class WithLongFunction : public rpp::synchronizable<WithLongFunction>
    {
    public:
        struct ValueType : public std::string
        {
            std::atomic_int associated_value = 0;
            int set_value_slow(int value, int sleep_for)
            {
                associated_value = value;
                sleep(sleep_for);
                return associated_value;
            }
        };
        ValueType value;
        rpp::mutex mutex;
        auto& get_mutex() { return mutex; }
        auto& get_ref() { return value; }
    };

    TestCase(sync_guard_locks_during_long_function_call)
    {
        WithLongFunction var;
        var->assign("Initial value");

        auto task = std::thread([&]
        {
            sleep(5);
            var->associated_value = 2;
        });

        // sets the value and holds the lock for a long time
        // the task will then enter the mutex and will be blocked,
        // unable to overwrite the value
        AssertEqual(var->set_value_slow(/*value*/1, /*sleep*/20), 1);

        task.join();
        AssertEqual(var->associated_value, 2);
    }

    TestCase(synchronized_var)
    {
        rpp::synchronized<std::string> str { "Initial value" };
        AssertEqual(*str, "Initial value");

        *str = "Testing operator*";
        AssertEqual(*str, "Testing operator*");

        str->assign("Testing operator->");
        AssertEqual(*str, "Testing operator->");

        *str = "Testing get()";
        AssertEqual((*str).get(), "Testing get()");

        auto guard = str.guard();
        guard = "First value";
        auto t = std::thread([&]
        {
            *str = "Second value";
        });
        for (int i = 0; i < 10; ++i)
        {
            AssertEqual(*guard, "First value");
            sleep(1);
        }
        guard.unlock();
        t.join();
        AssertEqual(*str, "Second value");
    }
};