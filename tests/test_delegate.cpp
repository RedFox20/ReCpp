//#define _DEBUG_FUNCTIONAL_MACHINERY
#include <rpp/delegate.h>
#include <rpp/stack_trace.h>
#include <rpp/tests.h>

namespace rpp
{
    // a generic data container for testing instances, functors and lambdas
    class Data
    {
    public:
        char* data = nullptr;
        static char* alloc(const char* str) {
            size_t n = strlen(str) + 1;
            return static_cast<char*>(memcpy(new char[n], str, n));
        }
        explicit Data(const char* s) : data(alloc(s)) {}
        Data()                       : data(alloc("data")) {}
        Data(const Data& d)          : data(alloc(d.data)) {}
        Data(Data&& d) noexcept      : data(d.data) { d.data = nullptr; }
        ~Data() { delete[] data; }
        Data& operator=(Data&& d) noexcept {
            std::swap(data, d.data);
            return *this;
        }
        Data& operator=(const Data& d) {
            if (this != &d) {
                delete[] data;
                data = alloc(d.data);
            }
            return *this;
        }
        bool operator==(const char* s) const { return strcmp(data, s) == 0; }
        bool operator!=(const char* s) const { return strcmp(data, s) != 0; }
    };
    string to_string(const Data& d) { return d.data; }

    #define VALIDATE_DATA_ARG(arg) \
        if (!(arg).data || (arg) != "data") \
            throw rpp::traced_exception{"Argument `"#arg"` did not contain \"data\""};

    static Data validate(const char* name, const Data& a)
    {
        printf("%s: '%s'\n", name, a.data);
        VALIDATE_DATA_ARG(a);
        return Data{name};
    }
    static Data validate(const char* name, const Data& a, const Data& b)
    {
        printf("%s: '%s' '%s'\n", name, a.data, b.data);
        VALIDATE_DATA_ARG(a);
        VALIDATE_DATA_ARG(b);
        return Data{name};
    }
    static Data validate(const char* name, const Data& a, const Data& b, const Data& c, const Data& d)
    {
        printf("%s: '%s' '%s' '%s' '%s'\n", name, a.data, b.data, c.data, d.data);
        VALIDATE_DATA_ARG(a);
        VALIDATE_DATA_ARG(b);
        VALIDATE_DATA_ARG(c);
        VALIDATE_DATA_ARG(d);
        return Data{name};
    }


    using DataDelegate = delegate<Data(Data a)>;

    inline string_buffer& operator<<(string_buffer& out, const DataDelegate& d)
    {
        return out << "delegate{" << d.get_fun() << "}";
    }

    TestImpl(test_delegate)
    {
        Data data;

        TestInit(test_delegate)
        {
        }

        ////////////////////////////////////////////////////

        TestCase(functions)
        {
            Data (*function)(Data a) = [](Data a) {
                return validate("function", a);
            };

            DataDelegate func = function;
            AssertThat(func.good(), true);
            AssertThat(func(data), "function");

            DataDelegate func2 = [](Data a) {
                return validate("function2", a);
            };
            AssertThat(func2.good(), true);
            AssertThat(func2(data), "function2");

            delegate<Data(const Data&)> func3 = [](const Data& a) {
                return validate("function3", a);
            };
            AssertThat(func3.good(), true);
            AssertThat(func3(data), "function3");
        }

        ////////////////////////////////////////////////////

        struct Base
        {
            Data x;
            virtual ~Base() = default;
            Data method(Data a) {
                return validate("method", a, x);
            }
            Data const_method(Data a) const {
                return validate("const_method", a, x);
            }
            virtual Data virtual_method(Data a) {
                return validate("virtual_method", a, x);
            }
        };
        struct Derived : Base {
            Data virtual_method(Data a) override {
                return validate("derived_method", a, x);
            }
        };

        TestCase(methods_bug)
        {
            using memb_type = Data (*)(void*, Data);
            struct dummy {};
            using dummy_type = Data (dummy::*)(Data a);
            union method_helper
            {
                memb_type mfunc;
                dummy_type dfunc;
            };

            Base inst;
            Data (Base::*method)(Data a) = &Base::method;

            void* obj = &inst;
            //printf("obj:  %p\n", obj);

            method_helper u;
            u.dfunc = reinterpret_cast<dummy_type>(method);

            auto* dum = static_cast<dummy*>(obj);
            (dum->*u.dfunc)(data);
        }

        TestCase(methods)
        {
            Derived inst;
            DataDelegate func1{inst, &Derived::method};
            AssertThat(func1(data), "method");

            DataDelegate func2{inst, &Derived::const_method};
            AssertThat(func2(data), "const_method");
        }

        TestCase(virtuals)
        {
            Base    base;
            Derived inst;

            // bind base virtual method
            DataDelegate func1(base, &Base::virtual_method);
            AssertThat(func1(data), "virtual_method");

            // bind virtual method directly
            DataDelegate func3(inst, &Derived::virtual_method);
            AssertThat(func3(data), "derived_method");

            // bind virtual method through type erasure
            Base& erased = inst;
            DataDelegate func2(erased, &Base::virtual_method);
            AssertThat(func2(data), "derived_method");
        }

        ////////////////////////////////////////////////////

        TestCase(lambdas)
        {
            DataDelegate lambda1 = [](Data a) {
                return validate("lambda1", a);
            };
            AssertThat(lambda1(data), "lambda1");

            DataDelegate lambda2 { [x=data](Data a) {
                return validate("lambda2", a, x);
            } };
            AssertThat(lambda2(data), "lambda2");
        }

        TestCase(nested_lambdas)
        {
            DataDelegate lambda = [x=data](Data a) {
                DataDelegate nested = [x=x](Data a) {
                    return validate("nested_lambda", x);
                };
                return nested(a);
            };
            AssertThat(lambda(data), "nested_lambda");

            DataDelegate moved_lambda = std::move(lambda);
            AssertThat(lambda.good(), false);
            AssertThat(moved_lambda(data), "nested_lambda");
        }

        TestCase(functor)
        {
            struct Functor {
                Data x;
                Data operator()(Data a) const {
                    return validate("functor", a, x);
                }
            };

            DataDelegate func = Functor{};
            AssertThat(func(data), "functor");
        }

        TestCase(move_init)
        {
            DataDelegate lambda = [x=data](Data a) {
                return validate("move_init", a);  
            };

            DataDelegate init { std::move(lambda) };
            AssertThat(init.good(), true);
            AssertThat(lambda.good(), false);
            AssertThat(init(data), "move_init");
        }

        TestCase(delegate_vector_push_back)
        {
            std::vector<DataDelegate> delegates;
            delegates.push_back([](Data a) { return validate("vector_0", a); });
            delegates.push_back([](Data a) { return validate("vector_1", a); });
            delegates.push_back([](Data a) { return validate("vector_2", a); });
            delegates.push_back([](Data a) { return validate("vector_3", a); });
            delegates.push_back([](Data a) { return validate("vector_4", a); });
            delegates.push_back([](Data a) { return validate("vector_5", a); });
            delegates.push_back([](Data a) { return validate("vector_6", a); });
            delegates.push_back([](Data a) { return validate("vector_7", a); });
            AssertThat(delegates[0](data), "vector_0");
            AssertThat(delegates[1](data), "vector_1");
            AssertThat(delegates[2](data), "vector_2");
            AssertThat(delegates[3](data), "vector_3");
            AssertThat(delegates[4](data), "vector_4");
            AssertThat(delegates[5](data), "vector_5");
            AssertThat(delegates[6](data), "vector_6");
            AssertThat(delegates[7](data), "vector_7");
        }

        TestCase(delegate_vector_emplace_back)
        {
            std::vector<DataDelegate> delegates;
            delegates.emplace_back([](Data a) { return validate("vector_0", a); });
            delegates.emplace_back([](Data a) { return validate("vector_1", a); });
            delegates.emplace_back([](Data a) { return validate("vector_2", a); });
            delegates.emplace_back([](Data a) { return validate("vector_3", a); });
            delegates.emplace_back([](Data a) { return validate("vector_4", a); });
            delegates.emplace_back([](Data a) { return validate("vector_5", a); });
            delegates.emplace_back([](Data a) { return validate("vector_6", a); });
            delegates.emplace_back([](Data a) { return validate("vector_7", a); });
            AssertThat(delegates[0](data), "vector_0");
            AssertThat(delegates[1](data), "vector_1");
            AssertThat(delegates[2](data), "vector_2");
            AssertThat(delegates[3](data), "vector_3");
            AssertThat(delegates[4](data), "vector_4");
            AssertThat(delegates[5](data), "vector_5");
            AssertThat(delegates[6](data), "vector_6");
            AssertThat(delegates[7](data), "vector_7");
        }

        TestCase(compare_empty)
        {
            DataDelegate empty;
            AssertThat(empty.good(), false);
            AssertThat(empty, nullptr);
            AssertThat(empty, DataDelegate{});
        }

        TestCase(compare_functions)
        {
            struct Compare {
                static Data some_function(Data a) {
                    return validate("compare_functions", a);
                }
                static Data another_function(Data a) {
                    return validate("another_function", a);
                }
            };

            DataDelegate func1 = &Compare::some_function;
            DataDelegate func2 { &Compare::some_function };
            AssertThat(func1.good(), true);
            AssertThat(func2.good(), true);
            AssertNotEqual(func1, nullptr);
            AssertNotEqual(func2, nullptr);
            AssertEqual(func1, func2);

            DataDelegate func3 { &Compare::another_function };
            AssertNotEqual(func1, func3);
        }

        TestCase(compare_lambdas)
        {
            auto compare_lambda = [](Data a) -> Data {
                return validate("compare_lambda", a);
            };
            auto compare_lambda2 = [](Data a) -> Data {
                return validate("compare_lambda2", a);
            };

            DataDelegate func1 = compare_lambda;
            DataDelegate func2 { compare_lambda };
            AssertThat(func1.good(), true);
            AssertThat(func2.good(), true);
            AssertNotEqual(func1, nullptr);
            AssertNotEqual(func2, nullptr);
            AssertNotEqual(func1, func2); // lambda delegates always copy the lambda state

            auto compare_lambda3 = compare_lambda2;
            DataDelegate func3 { compare_lambda2 };
            DataDelegate func4 { compare_lambda3 };
            AssertEqual(func3, func3);
            AssertNotEqual(func1, func3);
            AssertNotEqual(func3, func4);
        }

        TestCase(compare_methods)
        {
            Base inst, inst2;
            DataDelegate func1 {inst, &Base::method};
            DataDelegate func2 {inst, &Base::method};
            AssertThat(func1.good(), true);
            AssertThat(func2.good(), true);
            AssertNotEqual(func1, nullptr);
            AssertNotEqual(func2, nullptr);
            AssertEqual(func1, func2);

            DataDelegate func3 {inst,  &Base::const_method};
            DataDelegate func4 {inst2, &Base::const_method};
            AssertEqual(func3, func3);
            AssertNotEqual(func1, func3);
            AssertNotEqual(func3, func4);
        }

        ////////////////////////////////////////////////////

        static void event_func(Data a)
        {
            (void)validate("event_func", a);
        }

        TestCase(multicast_delegates)
        {
            struct Receiver
            {
                Data x;
                void event_method(Data a)
                {
                    (void)validate("event_method", a, x);
                }
                void const_method(Data a) const
                {
                    (void)validate("const_method", a, x);
                }
                void unused_method(Data a) const { const_method(a); }
            };

            Receiver receiver;
            multicast_delegate<Data> evt;
            AssertThat(evt.size(), 0); // yeah...

            // add 2 events
            evt += &event_func;
            evt.add(receiver, &Receiver::event_method);
            evt.add(receiver, &Receiver::const_method);
            evt(data);
            AssertThat(evt.size(), 3);

            // remove one event
            evt -= &event_func;
            evt(data);
            AssertThat(evt.size(), 2);

            // try to remove an incorrect function:
            evt -= &event_func;
            AssertThat(evt.size(), 2); // nothing must change
            evt.remove(receiver, &Receiver::unused_method);
            AssertThat(evt.size(), 2); // nothing must change

            // remove final events
            evt.remove(receiver, &Receiver::event_method);
            evt.remove(receiver, &Receiver::const_method);
            evt(data);
            AssertThat(evt.size(), 0); // must be empty now
            AssertThat(evt.empty(), true);
            AssertThat(evt.good(), false);
        }

        TestCase(multicast_delegate_copy_and_move)
        {
            int count = 0;
            multicast_delegate<Data> evt;
            evt += [&](Data a)
            {
                ++count;
                (void)validate("evt1", a); 
            };
            evt += [&](Data a)
            {
                ++count;
                (void)validate("evt2", a);
            };
            AssertThat(evt.empty(), false);
            AssertThat(evt.good(), true);
            AssertThat(evt.size(), 2);
            evt(data);
            AssertThat(count, 2);

            count = 0;
            multicast_delegate<Data> evt2 = evt;
            AssertThat(evt2.empty(), false);
            AssertThat(evt2.good(), true);
            AssertThat(evt2.size(), 2);
            evt2(data);
            AssertThat(count, 2);

            count = 0;
            multicast_delegate<Data> evt3 = std::move(evt2);
            AssertThat(evt3.empty(), false);
            AssertThat(evt3.good(), true);
            AssertThat(evt3.size(), 2);
            evt3(data);
            AssertThat(count, 2);
        }

        TestCase(std_function_args)
        {
            std::function<void(Data, Data&, const Data&, Data&&)> fun =
                [&](Data a, Data& b, const Data& c, Data&& d)
            {
                (void)validate("stdfun", a, b, c, d); 
            };
            Data copy = data;
            fun.operator()(data, data, data, std::move(copy));
        }

        TestCase(multicast_delegate_mixed_reference_args)
        {
            int count = 0;
            multicast_delegate<Data, Data&, const Data&, Data&&> evt;
            evt += [&](Data a, Data& b, const Data& c, Data&& d)
            {
                ++count;
                (void)validate("evt1", a, b, c, d); 
            };
            evt += [&](Data a, Data& b, const Data& c, Data&& d)
            {
                ++count;
                (void)validate("evt2", a, b, c, d); 
            };
            AssertThat(evt.empty(), false);
            AssertThat(evt.good(), true);
            AssertThat(evt.size(), 2);

            Data copy = data;
            evt(data, data, data, std::move(copy));
            AssertThat(count, 2);
        }

        ////////////////////////////////////////////////////
    };
    
}
