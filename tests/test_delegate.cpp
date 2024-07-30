//#define _DEBUG_FUNCTIONAL_MACHINERY
#include <rpp/delegate.h>
#include <rpp/stack_trace.h>
#include <functional>
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
            return static_cast<char*>(memcpy(malloc(n), str, n));
        }
        Data() noexcept : data(alloc("data")) {}
        Data(Data&& d) noexcept { std::swap(data, d.data); }
        Data(const Data& d) noexcept : data(alloc(d.data)) {}
        explicit Data(const char* s) noexcept : data(alloc(s)) {}
        ~Data() noexcept {
            if (data) {
                free(data);
                data = nullptr;
            }
        }
        Data& operator=(Data&& d) noexcept {
            if (this != &d)
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

    std::string to_string(const Data& d) noexcept { return d.data; }

    #define VALIDATE_DATA_ARG(name, arg) \
        if (!(arg).data || (arg) != "data") \
            throw rpp::traced_exception{std::string{name}+" argument `"#arg"` did not contain \"data\""};

    static Data validate(const char* name, const Data& a)
    {
        //printf("%s: '%s'\n", name, a.data);
        VALIDATE_DATA_ARG(name, a);
        return Data{name};
    }
    static Data validate(const char* name, const Data& a, const Data& b)
    {
        //printf("%s: '%s' '%s'\n", name, a.data, b.data);
        VALIDATE_DATA_ARG(name, a);
        VALIDATE_DATA_ARG(name, b);
        return Data{name};
    }
    static Data validate(const char* name, const Data& a, const Data& b, const Data& c, const Data& d)
    {
        //printf("%s: '%s' '%s' '%s' '%s'\n", name, a.data, b.data, c.data, d.data);
        VALIDATE_DATA_ARG(name, a);
        VALIDATE_DATA_ARG(name, b);
        VALIDATE_DATA_ARG(name, c);
        VALIDATE_DATA_ARG(name, d);
        return Data{name};
    }


    using DataDelegate = delegate<Data(Data a)>;

    inline string_buffer& operator<<(string_buffer& out, const DataDelegate& d)
    {
        return out << "delegate{" << d.get_obj() << "::" << d.get_fun() << "}";
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
            DataDelegate func1{&inst, &Derived::method};
            AssertThat(func1(data), "method");

            DataDelegate func2{&inst, &Derived::const_method};
            AssertThat(func2(data), "const_method");
        }

        TestCase(virtuals)
        {
            Base    base;
            Derived inst;

            // bind base virtual method
            DataDelegate func1(&base, &Base::virtual_method);
            AssertThat(func1(data), "virtual_method");

            // bind virtual method directly
            DataDelegate func3(&inst, &Derived::virtual_method);
            AssertThat(func3(data), "derived_method");

            // bind virtual method through type erasure
            Base& erased = inst;
            DataDelegate func2(&erased, &Base::virtual_method);
            AssertThat(func2(data), "derived_method");
        }

        ////////////////////////////////////////////////////

        /**
         * Using this convoluted Var<T>, since this is a simplification
         * of a real-world usecase from SyncVar<T> which triggers events via
         * delegate callbacks when a variable is synced over remote network.
         */
        template<class T> class TemplatedVar
        {
        public:
            T value {};
            T result {};
            rpp::delegate<void(const T& value)> func;
            TemplatedVar(T default_value, rpp::delegate<void(const T& value)>&& fn)
                : value{default_value}, result{}, func{std::move(fn)}
            {
            }
            void set_value(T new_value)
            {
                value = new_value;
                func(value);
            }
        };

        ////////////////////////////////////////////////////

        class VirtualInterfaceA
        {
        public:
            TemplatedVar<int> var_virtual_A;
            TemplatedVar<int> var_override_A;
            VirtualInterfaceA()
                : var_virtual_A{0, {this, &VirtualInterfaceA::virtual_method_A}}
                , var_override_A{0, {this, &VirtualInterfaceA::override_method_A}}
            {}
            virtual ~VirtualInterfaceA() = default;
            virtual void virtual_method_A(int value) noexcept { var_virtual_A.result = value; }
            virtual void override_method_A(int value) noexcept { var_override_A.result = value; }
        };

        class VirtualInterfaceB
        {
        public:
            TemplatedVar<int> var_virtual_B;
            TemplatedVar<int> var_override_B;
            VirtualInterfaceB()
                : var_virtual_B{0, {this, &VirtualInterfaceB::virtual_method_B}}
                , var_override_B{0, {this, &VirtualInterfaceB::override_method_B}}
            {}
            virtual ~VirtualInterfaceB() = default;
            virtual void virtual_method_B(int value) noexcept { var_virtual_B.result = value; }
            virtual void override_method_B(int value) noexcept { var_override_B.result = value; }
        };

        // In MSVC if a class uses multiple inheritance,
        // then we get this error:
        //       Pointers to members have different representations; cannot cast between them
        // This is because MSVC uses 16-byte PMF for multiple inheritance.
        class ContainingClass : public VirtualInterfaceA, public VirtualInterfaceB
        {
        public:
            TemplatedVar<int> var_byval;
            TemplatedVar<int> var_byref;
            TemplatedVar<int> var_subclass_override_A;
            TemplatedVar<int> var_subclass_override_B;

            ContainingClass()
                : var_byval{0, {this, &ContainingClass::var1_byval_method}}
                , var_byref{0, {this, &ContainingClass::var2_byref_method}}
                , var_subclass_override_A{0, {this, &ContainingClass::override_method_A}}
                , var_subclass_override_B{0, {this, &ContainingClass::override_method_B}}
            {}
            void var1_byval_method(int value) noexcept { var_byval.result = value; }
            void var2_byref_method(const int& value) noexcept { var_byref.result = value; }
            void override_method_A(int value) noexcept override {
                var_override_A.result = value*2;
                var_subclass_override_A.result = value*2;
            }
            void override_method_B(int value) noexcept final {
                var_override_B.result = value*3;
                var_subclass_override_B.result = value*3;
            }
        };

        TestCase(multi_inheritance_pmf_resolves_correctly)
        {
            ContainingClass obj;

            // calling by value works
            obj.var_byval.set_value(42);
            AssertThat(obj.var_byval.result, 42);

            // calling by ref works
            obj.var_byref.set_value(22);
            AssertThat(obj.var_byref.result, 22);
        }

        TestCase(multi_inheritance_virtual_pmf_resolves_as_expected)
        {
            ContainingClass obj;

            // calling non-overridden virtual A works
            obj.var_virtual_A.set_value(11);
            AssertThat(obj.var_virtual_A.result, 11);

            // calling non-overridden virtual B works
            obj.var_virtual_B.set_value(33);
            AssertThat(obj.var_virtual_B.result, 33);

            // calling overridden virtual A will use the override method, not the base class method
            obj.var_override_A.set_value(5);
            AssertThat(obj.var_override_A.result, 10);

            // calling final virtual B will use the final method, not the base class method
            obj.var_override_B.set_value(7);
            AssertThat(obj.var_override_B.result, 21);

            // when initialized from subclass directly, it should always resolve to the override
            obj.var_subclass_override_A.set_value(3);
            AssertThat(obj.var_subclass_override_A.result, 6);

            obj.var_subclass_override_B.set_value(4);
            AssertThat(obj.var_subclass_override_B.result, 12);
        }

        ////////////////////////////////////////////////////

        class ConstRefAdapterClass
        {
        public:
            mutable int result = 0;

            void cref_method(const int& value) noexcept { result = value; }
            void byval_method(int value) noexcept { result = value; }

            void cref_const_method(const int& value) const noexcept { result = value; }
            void byval_const_method(int value) const noexcept { result = value; }
        };

        // ensures cref arguments are correctly adapted to byval arguments via an adapter call
        TestCase(decay_adapter_method_cref_to_byval)
        {
            ConstRefAdapterClass obj1;
            rpp::delegate<void(int val)> func1 {&obj1, &ConstRefAdapterClass::cref_method};
            func1(42);
            AssertThat(obj1.result, 42);

            ConstRefAdapterClass obj2;
            rpp::delegate<void(int val)> func2 {&obj2, &ConstRefAdapterClass::cref_const_method};
            func2(89);
            AssertThat(obj2.result, 89);
        }

        TestCase(decay_adapter_method_byval_to_cref)
        {
            ConstRefAdapterClass obj1;
            rpp::delegate<void(const int& val)> func1 {&obj1, &ConstRefAdapterClass::byval_method};
            func1(42);
            AssertThat(obj1.result, 42);

            ConstRefAdapterClass obj2;
            rpp::delegate<void(const int& val)> func2 {&obj2, &ConstRefAdapterClass::byval_const_method};
            func2(89);
            AssertThat(obj2.result, 89);
        }

        // no actual decay happens, but everything should still work correctly
        TestCase(decay_adapter_method_cref_noop)
        {
            ConstRefAdapterClass obj1;
            rpp::delegate<void(const int& val)> func1 {&obj1, &ConstRefAdapterClass::cref_method};
            rpp::delegate<void(const int& val)> func2 {&obj1, &ConstRefAdapterClass::cref_const_method};
            func1(42);
            AssertThat(obj1.result, 42);
            func2(89);
            AssertThat(obj1.result, 89);

        }

        TestCase(decay_adapter_method_noop)
        {
            ConstRefAdapterClass obj1;
            rpp::delegate<void(int val)> func1 {&obj1, &ConstRefAdapterClass::byval_method};
            rpp::delegate<void(int val)> func2 {&obj1, &ConstRefAdapterClass::byval_const_method};
            func1(42);
            AssertThat(obj1.result, 42);
            func2(89);
            AssertThat(obj1.result, 89);
        }

        TestCase(decay_adapter_lambda_cref_to_byval)
        {
            // cref lambda could incorrectly decay to byval, causing
            // pointer-to-integer be passed as `int val`.
            // this must be handled by rpp::delegate by adding a proxy adapter.
            int result = 0;
            rpp::delegate<void(int val)> func1 = [&](const int& val) {
                result = val;
            };
            func1(42);
            AssertThat(result, 42);

            result = 0;
            rpp::delegate<void(const int& val)> func2 = [&](int val) {
                result = val;
            };
            func2(22);
            AssertThat(result, 22);
        }

        TestCase(decay_adapter_function_cref_to_byval)
        {
            static int int_result = 0;
            static std::string str_result;
            struct cref
            {
                static void int_func(const int& val) noexcept { int_result = val; }
                static void str_func(const std::string& val) noexcept { str_result = val; }
            };

            int_result = 0;
            auto byval_int_func = rpp::delegate<void(int val)>{ &cref::int_func };
            int value = 4141;
            byval_int_func(value);
            AssertThat(int_result, 4141);

            str_result = {};
            auto byval_str_func = rpp::delegate<void(std::string val)>{ &cref::str_func };
            std::string str = "dynamically allocated long test string";
            byval_str_func(str);
            AssertThat(str_result, "dynamically allocated long test string");
        }

        TestCase(decay_adapter_function_byval_to_cref)
        {
            static int int_result = 0;
            static std::string str_result;
            struct byval
            {
                static void int_func(int val) noexcept { int_result = val; }
                static void str_func(std::string val) noexcept { str_result = val; }
            };

            int_result = 0;
            auto byref_int_func = rpp::delegate<void(const int& val)>{ &byval::int_func };
            int value = 4242;
            byref_int_func(value);
            AssertThat(int_result, 4242);

            str_result = {};
            auto byref_str_func = rpp::delegate<void(const std::string& val)>{ &byval::str_func };
            std::string str = "dynamically allocated long test string";
            byref_str_func(str);
            AssertThat(str_result, "dynamically allocated long test string");
        }

        TestCase(decay_adapter_function_noop)
        {
            static int int_result = 0;
            static std::string str_result;
            struct noop
            {
                static void int_func(int val) noexcept { int_result = val; }
                static void str_func(std::string val) noexcept { str_result = val; }
            };

            int_result = 0;
            auto noop_int_func = rpp::delegate<void(int val)>{ &noop::int_func };
            int value = 4242;
            noop_int_func(value);
            AssertThat(int_result, 4242);

            str_result = {};
            auto noop_str_func = rpp::delegate<void(std::string val)>{ &noop::str_func };
            std::string str = "dynamically allocated long test string";
            noop_str_func(str);
            AssertThat(str_result, "dynamically allocated long test string");
        }

        ////////////////////////////////////////////////////

        TestCase(basic_lambda)
        {
            DataDelegate lambda1 = [x=1](Data a) {
                (void)x;
                return validate("lambda1", a);
            };
            Data result = lambda1.invoke(data);
            AssertThat(result, "lambda1");

            DataDelegate lambda2 { [x=data](Data a) {
                return validate("lambda2", a, x);
            } };
            AssertThat(lambda2(data), "lambda2");
        }

        using StringOp = rpp::delegate<std::string(std::string a, std::string b)>;

        TestCase(lambda_returning_data)
        {
            StringOp join1 = [](const std::string& a, const std::string& b) {
                return a + b;
            };
            std::string joined1 = join1("long string will be joined", " with another string of similar length");
            AssertThat(joined1, "long string will be joined with another string of similar length");

            std::string capture = " and an extra capture string which is appended";
            StringOp join2 = [capture](std::string a, std::string b) {
                return a + b + capture;
            };
            std::string joined2 = join2("long string will be joined", " with another string of similar length");
            AssertThat(joined2, "long string will be joined with another string of similar length and an extra capture string which is appended");
        }

        TestCase(lambda_nested)
        {
            DataDelegate lambda = [x=data](Data a) {
                DataDelegate nested = [x=x](Data a) {
                    (void)a;
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

        TestCase(lambda_move_init)
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
            // add state to lambda, so it is not optimized into a function pointer
            auto compare_lambda = [x=0](Data a) -> Data {
                (void)x;
                return validate("compare_lambda", a);
            };
            auto compare_lambda2 = [y=1](Data a) -> Data {
                (void)y;
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
            DataDelegate func1 {&inst, &Base::method};
            DataDelegate func2 {&inst, &Base::method};
            AssertThat(func1.good(), true);
            AssertThat(func2.good(), true);
            AssertNotEqual(func1, nullptr);
            AssertNotEqual(func2, nullptr);
            AssertEqual(func1, func2);

            DataDelegate func3 {&inst,  &Base::const_method};
            DataDelegate func4 {&inst2, &Base::const_method};
            AssertEqual(func3, func3);
            AssertNotEqual(func1, func3);
            AssertNotEqual(func3, func4);
        }

        TestCase(copy_operator_lambdas)
        {
            auto lambda = [state=1](Data a) -> Data {
                (void)state;
                return validate("copy_lambda", a);
            };

            DataDelegate original { lambda };
            DataDelegate copied;
            copied = original; // explicitly test copy operator here
            AssertThat(original.good(), true);
            AssertThat(copied.good(), true);

            AssertEqual(original(data), "copy_lambda");
            AssertEqual(copied(data), "copy_lambda");
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
            evt.add(&receiver, &Receiver::event_method);
            evt.add(&receiver, &Receiver::const_method);
            evt(data);
            AssertThat(evt.size(), 3);

            // remove one event
            evt -= &event_func;
            evt(data);
            AssertThat(evt.size(), 2);

            // try to remove an incorrect function:
            evt -= &event_func;
            AssertThat(evt.size(), 2); // nothing must change
            evt.remove(&receiver, &Receiver::unused_method);
            AssertThat(evt.size(), 2); // nothing must change

            // remove final events
            evt.remove(&receiver, &Receiver::event_method);
            evt.remove(&receiver, &Receiver::const_method);
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
