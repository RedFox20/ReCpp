//#define _DEBUG_FUNCTIONAL_MACHINERY
#include <rpp/delegate.h>
#include <rpp/stack_trace.h>
#include <cstring> // strlen, strcmp
#include <functional>
#include <memory> // std::make_shared
#include <stdexcept> // std::invalid_argument
#include <string>
#include <vector>
#include <rpp/tests.h>

namespace rpp
{
    // NOLINTBEGIN(performance-*,readability-make-member-function-const,bugprone-exception-escape)

    // a generic data container for testing instances, functors and lambdas
    class Data
    {
    public:
        char* data = nullptr;
        static char* alloc(const char* str) {
            size_t n = strlen(str) + 1;
            void* mem = malloc(n);
            if (!mem) { AssertFailed("Memory allocation failed"); return nullptr; }
            return static_cast<char*>(memcpy(mem, str, n));
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

        TestCase(init_nullptr)
        {
            rpp::delegate<void(std::string val)> func = nullptr;
            AssertThat(func.good(), false);
            AssertTrue(!func);
            AssertThat(func, nullptr);
            AssertThat(func.get_fun(), nullptr);

            func = nullptr;
            AssertThat(func.good(), false);
            AssertTrue(!func);
            AssertThat(func, nullptr);
            AssertThat(func.get_fun(), nullptr);
        }

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

        // reset() shares the not_copy_ctor constraint with the master constructor,
        // and it dispatches the same three ways
        TestCase(reset_takes_every_callable_shape)
        {
            Data (*function)(Data a) = [](Data a) { return validate("function", a); };

            DataDelegate func;
            func.reset(function); // function pointer
            AssertThat(func(data), "function");

            func.reset([](Data a) { return validate("lambda", a); }); // functor
            AssertThat(func(data), "lambda");

            Derived inst;
            func.reset(&inst, &Derived::method); // member overload, not the template
            AssertThat(func(data), "method");
            func.reset(&inst, &Derived::const_method);
            AssertThat(func(data), "const_method");

            func.reset(nullptr);
            AssertThat(func.good(), false);
        }

        // not_copy_ctor keeps a delegate argument out of the master constructor, which would
        // otherwise beat the copy constructor for a non-const lvalue
        TestCase(copy_construction_prefers_the_copy_constructor)
        {
            DataDelegate original = [](Data a) { return validate("original", a); };

            DataDelegate from_lvalue { original };
            AssertThat(from_lvalue(data), "original");
            AssertThat(original.good(), true); // the copy left the source intact

            const DataDelegate& cref = original;
            DataDelegate from_const_lvalue { cref };
            AssertThat(from_const_lvalue(data), "original");

            DataDelegate from_rvalue { std::move(original) };
            AssertThat(from_rvalue(data), "original");
        }

        // the same constraint on operator=, driven from the callable side
        TestCase(assignment_takes_every_callable_shape)
        {
            Data (*function)(Data a) = [](Data a) { return validate("function", a); };

            DataDelegate func;
            func = function;
            AssertThat(func(data), "function");

            func = [](Data a) { return validate("lambda", a); };
            AssertThat(func(data), "lambda");

            func = nullptr;
            AssertThat(func.good(), false);
        }

        // only the const-instance constructors accept a const object, and args_match picks
        // the direct call or the adapter between them
        TestCase(const_instance_binds_the_const_method_constructors)
        {
            const Derived inst;
            DataDelegate direct { &inst, &Derived::const_method }; // args match exactly
            AssertThat(direct(data), "const_method");

            const ConstRefAdapterClass obj;
            rpp::delegate<void(int val)> adapted { &obj, &ConstRefAdapterClass::cref_const_method };
            adapted(42); // const int& against int, so this one takes the adapter
            AssertThat(obj.result, 42);

            rpp::delegate<void(int val)> exact { &obj, &ConstRefAdapterClass::byval_const_method };
            exact(89);
            AssertThat(obj.result, 89);
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

        struct Left
        {
            int left = 1;
            virtual ~Left() = default;
            virtual int left_virtual(int x) { return x + left; }
        };
        struct Right
        {
            int right = 100;
            virtual ~Right() = default;
            int right_method(int x) { return x + right; }
            int right_const(int x) const { return x + right; }
            virtual int right_virtual(int x) { return x + right; }
        };
        struct Overrides : Left, Right
        {
            int own = 1000;
            int right_virtual(int x) override { return x + own; }
        };
        struct Inherits : Left, Right {};

        // Right sits at a non-zero offset, so every call needs the Right subobject as `this`
        TestCase(non_primary_base_method_binds_the_base_subobject)
        {
            Inherits inh;
            Overrides over;
            AssertThat(rpp::delegate<int(int)>(&inh, &Inherits::right_method)(1), 101);
            AssertThat(rpp::delegate<int(int)>(&inh, &Inherits::right_virtual)(1), 101);
            AssertThat(rpp::delegate<int(int)>(&inh, &Inherits::left_virtual)(1), 2);
            AssertThat(rpp::delegate<int(int)>(&over, &Overrides::right_virtual)(1), 1001);
            AssertThat(rpp::delegate<int(int)>(&over, &Right::right_virtual)(1), 1001);
            Right* erased = &over;
            AssertThat(rpp::delegate<int(int)>(erased, &Right::right_virtual)(1), 1001);

            const Inherits cinh;
            AssertThat(rpp::delegate<int(int)>(&cinh, &Inherits::right_const)(1), 101);
            AssertThat(rpp::delegate<int(const int&)>(&inh, &Inherits::right_method)(1), 101); // the adapter

            rpp::delegate<int(int)> reset;
            reset.reset(&inh, &Inherits::right_method);
            AssertThat(reset(1), 101);
            AssertThat(reset.equals(&inh, &Inherits::right_method), true);
            reset.reset(&cinh, &Inherits::right_const);
            AssertThat(reset(1), 101);
            AssertThat(reset.equals(&cinh, &Inherits::right_const), true);
        }

        // a derived member pointer to a base method carries the this adjustment inside it
        TestCase(member_pointer_with_this_adjustment)
        {
            Inherits inh;
            Overrides over;
            int (Inherits::*method)(int) = &Right::right_method;
            int (Inherits::*virt)(int) = &Right::right_virtual;
            int (Overrides::*over_virt)(int) = &Right::right_virtual;
            AssertThat(rpp::delegate<int(int)>(&inh, method)(1), 101);
            AssertThat(rpp::delegate<int(int)>(&inh, virt)(1), 101);
            AssertThat(rpp::delegate<int(int)>(&over, over_virt)(1), 1001);

            rpp::delegate<int(int)> d { &inh, method };
            AssertThat(d.equals(&inh, method), true);
            AssertThat(d.equals(&inh, virt), false);
        }

        struct Shared
        {
            int shared = 7;
            int last = 0;
            virtual ~Shared() = default;
            int shared_method(int x) { return x + shared; }
            virtual int shared_virtual(int x) { return x + shared; }
            virtual void shared_event(int x) { last = x + shared; }
        };
        struct VirtualChild : Left, virtual Shared
        {
            int shared_virtual(int x) override { return x + 70; }
            void shared_event(int x) override { last = x + 70; }
        };

        // MSVC calls a virtual base method through the adapter, so equals() and remove() must still match it
        TestCase(virtual_base_method_compares_and_removes)
        {
            VirtualChild child;
            rpp::delegate<int(int)> method { &child, &VirtualChild::shared_virtual };
            AssertThat(method.equals(&child, &VirtualChild::shared_virtual), true);

            multicast_delegate<int> evt;
            evt.add(&child, &VirtualChild::shared_event);
            evt(1);
            AssertThat(child.last, 71);
            evt.remove(&child, &VirtualChild::shared_event);
            AssertThat(evt.size(), 0);
        }

        TestCase(virtual_base_method_binds_the_base_subobject)
        {
            VirtualChild child;
            AssertThat(rpp::delegate<int(int)>(&child, &VirtualChild::shared_method)(1), 8);
            AssertThat(rpp::delegate<int(int)>(&child, &Shared::shared_virtual)(1), 71);
            AssertThat(rpp::delegate<int(int)>(&child, &VirtualChild::shared_virtual)(1), 71);
            Shared* erased = &child;
            AssertThat(rpp::delegate<int(int)>(erased, &Shared::shared_virtual)(1), 71);
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
                : value{default_value}, func{std::move(fn)}
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
                static void str_func(std::string val) noexcept { str_result = val; } // NOLINT(performance-unnecessary-value-param)
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
            AssertThat(lambda.good(), false); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
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
            AssertThat(lambda.good(), false); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
            AssertThat(init(data), "move_init");
        }

        TestCase(delegate_vector_push_back)
        {
            std::vector<DataDelegate> delegates;
            // NOLINTBEGIN(modernize-use-emplace)
            delegates.push_back([](Data a) { return validate("vector_0", a); });
            delegates.push_back([](Data a) { return validate("vector_1", a); });
            delegates.push_back([](Data a) { return validate("vector_2", a); });
            delegates.push_back([](Data a) { return validate("vector_3", a); });
            delegates.push_back([](Data a) { return validate("vector_4", a); });
            delegates.push_back([](Data a) { return validate("vector_5", a); });
            delegates.push_back([](Data a) { return validate("vector_6", a); });
            delegates.push_back([](Data a) { return validate("vector_7", a); });
            // NOLINTEND(modernize-use-emplace)
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
            Base inst, inst2; // NOLINT(readability-isolate-declaration)
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

        TestCase(copy_keeps_the_captures_of_the_source)
        {
            auto state = std::make_shared<int>(42);
            auto lambda = [state] { return state ? *state : -1; }; // a moved-out capture reads as a sentinel, not a crash

            rpp::delegate<int()> original { lambda };
            rpp::delegate<int()> copied { original };
            AssertThat(original(), 42);
            copied = original;
            AssertThat(original(), 42);
            AssertThat(copied(), 42);

            original = lambda;
            AssertThat(lambda(), 42);
            original.reset(lambda);
            AssertThat(lambda(), 42);
            AssertThat(state.use_count(), 4); // the owners are state, lambda, original and copied
        }

        ////////////////////////////////////////////////////

        static void event_func(Data a)
        {
            (void)validate("event_func", a);
        }
        static void event_func_int(int /*value*/) {}

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
                [&](Data a, Data& b, const Data& c, Data&& d) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
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
            evt += [&](Data a, Data& b, const Data& c, Data&& d) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
            {
                ++count;
                (void)validate("evt1", a, b, c, d); 
            };
            evt += [&](Data a, Data& b, const Data& c, Data&& d) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
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

        // a function source overwrote the pointers and left the old functor allocated,
        // which ASAN reported as a leak. see BUGS.md C29
        TestCase(copy_assign_from_function_frees_the_old_functor)
        {
            static int destroyed = 0;
            destroyed = 0;
            struct tracked
            {
                int payload = 0;
                ~tracked() { ++destroyed; }
                void operator()() const noexcept {}
            };
            struct plain_source { static void func() noexcept {} };

            rpp::delegate<void()> held { tracked{} };
            const rpp::delegate<void()> plain { &plain_source::func };
            const int base = destroyed; // the source temporary already died

            held = plain; // the functor `held` owns must be freed here, not leaked
            AssertThat(destroyed, base + 1);

            held = plain; // a second assign has nothing left to free
            AssertThat(destroyed, base + 1);

            // a self copy must survive: both branches free the destination before they read
            plain.copy(const_cast<rpp::delegate<void()>&>(plain));
            AssertThat(plain.good(), true);

            rpp::delegate<void()> functor { tracked{} };
            functor.copy(functor);
            AssertThat(functor.good(), true);
            functor(); // the functor is still callable, not freed
        }

        ////////////////////////////////////////////////////

        TestCase(null_function_pointer_makes_an_empty_delegate)
        {
            Data (*function)(Data a) = nullptr;
            DataDelegate func = function;
            AssertThat(func.good(), false);
            AssertThat(func.get_obj(), nullptr);

            func = [](Data a) { return validate("lambda", a); };
            func.reset(function);
            AssertThat(func.good(), false);
        }

        TestCase(null_instance_throws_or_resets)
        {
            Derived* none = nullptr;
            const Derived* cnone = nullptr;
            ConstRefAdapterClass* anone = nullptr;
            const ConstRefAdapterClass* canone = nullptr;
            AssertThrows(DataDelegate(none, &Derived::method), std::invalid_argument);
            AssertThrows(DataDelegate(cnone, &Derived::const_method), std::invalid_argument);
            AssertThrows(rpp::delegate<void(int)>(anone, &ConstRefAdapterClass::cref_method), std::invalid_argument);
            AssertThrows(rpp::delegate<void(int)>(canone, &ConstRefAdapterClass::cref_const_method), std::invalid_argument);

            Derived inst;
            DataDelegate func { &inst, &Derived::method };
            func.reset(none, &Derived::method);
            AssertThat(func.good(), false);
            func.reset(&inst, &Derived::const_method);
            func.reset(cnone, &Derived::const_method);
            AssertThat(func.good(), false);
            AssertThat(func.equals(none, &Derived::method), true); // an empty delegate equals a null instance
            AssertThat(func.equals(cnone, &Derived::const_method), true);
        }

        TestCase(self_assignment_keeps_the_target)
        {
            DataDelegate func = [x=data](Data a) { return validate("self", a, x); };
            DataDelegate& alias = func;
            func = alias;
            AssertThat(func(data), "self");
            func = std::move(alias);
            AssertThat(func(data), "self");
        }

        TestCase(reset_from_another_delegate)
        {
            const DataDelegate lambda = [x=data](Data a) { return validate("lambda", a, x); };
            DataDelegate func;
            func.reset(lambda);
            AssertThat(func(data), "lambda");
            AssertThat(lambda(data), "lambda");

            DataDelegate moved = lambda;
            func.reset(std::move(moved));
            AssertThat(func(data), "lambda");
        }

        TestCase(equals_compares_the_target_and_the_instance)
        {
            struct Functor { Data operator()(Data a) const { return validate("functor", a); } };
            Derived inst, inst2; // NOLINT(readability-isolate-declaration)
            DataDelegate method { &inst, &Derived::method };
            AssertThat(method.equals(&inst, &Derived::method), true);
            AssertThat(method.equals(&inst2, &Derived::method), false);
            AssertThat(method.equals(&inst, &Derived::virtual_method), false);
            AssertThat(method.equals(DataDelegate{ &inst, &Derived::method }), true);
            AssertThat(method.get_obj(), static_cast<void*>(&inst));

            const Derived& cinst = inst;
            DataDelegate cmethod { &cinst, &Derived::const_method };
            AssertThat(cmethod.equals(&cinst, &Derived::const_method), true);
            AssertThat(cmethod.equals(&inst, &Derived::method), false);

            DataDelegate functor = Functor{};
            AssertThat(functor.equals<Functor>(), true);
            AssertThat(method.equals<Functor>(), false);
            AssertThat(functor(data), "functor");
        }

        // a move-only functor cannot copy, so a copy takes the state of the source
        TestCase(copy_of_a_move_only_functor_takes_the_state)
        {
            auto value = std::make_unique<int>(42);
            rpp::delegate<int()> source = [value=std::move(value)] { return value ? *value : -1; };
            rpp::delegate<int()> copied { source };
            AssertThat(copied(), 42);
            AssertThat(source(), -1);
        }

        ////////////////////////////////////////////////////

        template<class D> static bool is_inline(const D& d)
        {
            const uintptr_t self = reinterpret_cast<uintptr_t>(&d);
            const uintptr_t functor = reinterpret_cast<uintptr_t>(d.get_obj());
            return self <= functor && functor < self + sizeof(D);
        }

        TestCase(delegate_stays_five_pointers)
        {
            AssertThat(sizeof(rpp::delegate<void()>), 5 * sizeof(void*));
            AssertThat(sizeof(DataDelegate), 5 * sizeof(void*));
        }

        TestCase(small_trivially_copyable_functors_live_inline)
        {
            size_t a = 1, b = 2, c = 3; // NOLINT(readability-isolate-declaration)
            rpp::delegate<size_t(size_t)> cap0 = [](size_t x) { return x; };
            rpp::delegate<size_t(size_t)> cap1 = [a](size_t x) { return x + a; };
            rpp::delegate<size_t(size_t)> cap2 = [a, b](size_t x) { return x + a + b; };
            rpp::delegate<size_t(size_t)> cap3 = [a, b, c](size_t x) { return x + a + b + c; };
            rpp::delegate<size_t(size_t)> refs = [&a, &b, &c](size_t x) { return x + a + b + c; };
            AssertThat(is_inline(cap0) && is_inline(cap1) && is_inline(cap2), true);
            AssertThat(is_inline(cap3) && is_inline(refs), true);
            AssertThat(cap0(10) + cap1(10) + cap2(10) + cap3(10) + refs(10), size_t(10 + 11 + 13 + 16 + 16));

            ConstRefAdapterClass adapted;
            rpp::delegate<void(int)> adapter { &adapted, &ConstRefAdapterClass::cref_method };
            AssertThat(is_inline(adapter), true); // the adapter captures the instance and the method
            adapter(5);
            AssertThat(adapted.result, 5);
        }

        TestCase(large_or_non_trivial_functors_live_on_the_heap)
        {
            size_t a = 1, b = 2, c = 3, d = 4, e = 5, f = 6, g = 7; // NOLINT(readability-isolate-declaration)
            rpp::delegate<size_t()> cap4 = [a, b, c, d] { return a + b + c + d; };
            rpp::delegate<size_t()> cap7 = [a, b, c, d, e, f, g] { return a + b + c + d + e + f + g; };
            AssertThat(is_inline(cap4) || is_inline(cap7), false);
            AssertThat(cap4() + cap7(), size_t(10 + 28));

            std::string s1 = "a long string which needs a heap buffer", s2 = s1 + "!", s3 = s2 + "!"; // NOLINT(readability-isolate-declaration)
            rpp::delegate<size_t()> str1 = [s1] { return s1.size(); };
            rpp::delegate<size_t()> str3 = [s1, s2, s3] { return s1.size() + s2.size() + s3.size(); };
            AssertThat(is_inline(str1) || is_inline(str3), false);
            AssertThat(str1(), s1.size());
            AssertThat(str3(), s1.size() + s2.size() + s3.size());

            auto shared = std::make_shared<int>(7);
            rpp::delegate<int()> ptr = [shared] { return *shared; }; // fits, but is not trivially copyable
            AssertThat(is_inline(ptr), false);
            AssertThat(ptr(), 7);

            struct alignas(64) aligned { int value = 9; int operator()() const { return value; } };
            rpp::delegate<int()> over = aligned{};
            AssertThat(is_inline(over), false);
            AssertThat(reinterpret_cast<uintptr_t>(over.get_obj()) % 64, uintptr_t(0));
            AssertThat(over(), 9);
        }

        // copy_bits must point `obj` at the storage of the destination, never at the source
        TestCase(inline_functor_moves_and_copies_into_the_destination)
        {
            int x = 1, y = 2; // NOLINT(readability-isolate-declaration)
            rpp::delegate<int()> a = [x] { return x; };
            rpp::delegate<int()> b = [y] { return y * 10; };

            rpp::delegate<int()> moved { std::move(a) };
            AssertThat(is_inline(moved), true);
            AssertThat(a.good(), false); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
            AssertThat(moved(), 1);

            moved = std::move(b); // the move assignment swaps
            AssertThat(is_inline(moved) && is_inline(b), true); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
            AssertThat(moved(), 20);
            AssertThat(b(), 1); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)

            rpp::delegate<int()> copied { moved };
            moved = [] { return -1; };
            AssertThat(is_inline(copied), true);
            AssertThat(copied(), 20);

            copied = b;
            b = nullptr;
            AssertThat(copied(), 1);
        }

        // the swap in the move assignment pairs every kind with every other kind
        TestCase(move_assignment_swaps_every_kind)
        {
            Data (*function)(Data a) = [](Data a) { return validate("function", a); };
            Derived inst;
            int v = 4;
            std::string big = "a long string which needs a heap buffer";
            auto make = [&](int kind) -> rpp::delegate<Data(Data)> {
                switch (kind) {
                    case 0: return {};
                    case 1: return function;
                    case 2: return { &inst, &Derived::method };
                    case 3: return [v](Data a) { return validate(v == 4 ? "inline" : "bad", a); };
                    default: return [big](Data a) { return validate(big.size() > 30 ? "heap" : "bad", a); };
                }
            };
            const char* names[] = { "", "function", "method", "inline", "heap" };
            for (int i = 0; i < 5; ++i)
            {
                for (int j = 0; j < 5; ++j)
                {
                    rpp::delegate<Data(Data)> left = make(i);
                    rpp::delegate<Data(Data)> right = make(j);
                    left = std::move(right);
                    AssertThat(left.good(), j != 0);
                    AssertThat(right.good(), i != 0); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
                    if (j != 0) AssertThat(left(data), names[j]);
                    if (i != 0) AssertThat(right(data), names[i]); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
                    if (j == 3) AssertThat(is_inline(left), true);
                    if (i == 3) AssertThat(is_inline(right), true);
                }
            }
        }

        TestCase(heap_functor_lifetime_follows_the_owner)
        {
            auto state = std::make_shared<int>(5);
            {
                rpp::delegate<int()> original = [state] { return *state; };
                AssertThat(state.use_count(), 2);
                rpp::delegate<int()> copied { original };
                AssertThat(state.use_count(), 3);
                rpp::delegate<int()> moved { std::move(copied) };
                AssertThat(state.use_count(), 3); // the move steals the heap pointer
                copied = moved;
                AssertThat(state.use_count(), 4);
                moved.reset();
                AssertThat(state.use_count(), 3);
                original = [] { return 0; };
                AssertThat(state.use_count(), 2);
                AssertThat(copied(), 5);
            }
            AssertThat(state.use_count(), 1);
        }

        TestCase(vector_growth_relocates_inline_functors)
        {
            std::vector<rpp::delegate<int()>> delegates;
            for (int i = 0; i < 33; ++i)
                delegates.emplace_back([i] { return i; });
            int sum = 0;
            for (int i = 0; i < 33; ++i)
            {
                AssertThat(delegates[i](), i);
                sum += is_inline(delegates[i]) ? 1 : 0;
            }
            AssertThat(sum, 33);
        }

        // a removal shifts the inline functors after it, and each one must keep its own state
        TestCase(multicast_delegate_relocates_inline_functors)
        {
            std::vector<int> log;
            Recorder rec { 99, &log };
            multicast_delegate<int> evt;
            for (int i = 0; i < 4; ++i)
                evt += [&log, i](int x) { log.push_back(i * 100 + x); };
            evt.add(&rec, &Recorder::on_event);
            for (int i = 4; i < 9; ++i)
                evt += [&log, i](int x) { log.push_back(i * 100 + x); };

            evt.remove(&rec, &Recorder::on_event);
            evt(1);
            AssertThat(log, (std::vector<int>{ 1, 101, 201, 301, 401, 501, 601, 701, 801 }));

            multicast_delegate<int> copy = evt;
            log.clear();
            copy(2);
            AssertThat(log, (std::vector<int>{ 2, 102, 202, 302, 402, 502, 602, 702, 802 }));
        }

        ////////////////////////////////////////////////////

        struct Recorder
        {
            int id = 0;
            std::vector<int>* log = nullptr;
            void on_event(int x) { log->push_back(id * 100 + x); }
        };

        TestCase(multicast_delegate_grows_and_removes_in_order)
        {
            std::vector<int> log;
            Recorder rec[9];
            multicast_delegate<int> evt;
            for (int i = 0; i < 9; ++i)
            {
                rec[i] = Recorder{ i, &log };
                evt.add(&rec[i], &Recorder::on_event);
            }
            AssertThat(evt.size(), 9);
            evt(1);
            AssertThat(log, (std::vector<int>{ 1, 101, 201, 301, 401, 501, 601, 701, 801 }));

            evt.remove(static_cast<Recorder*>(nullptr), &Recorder::on_event); // a null instance matches nothing
            AssertThat(evt.size(), 9);
            evt.remove(&rec[4], &Recorder::on_event); // the middle
            evt.remove(&rec[0], &Recorder::on_event); // the first
            evt.remove(&rec[8], &Recorder::on_event); // the last
            log.clear();
            evt.invoke(2);
            AssertThat(log, (std::vector<int>{ 102, 202, 302, 502, 602, 702 }));

            const multicast_delegate<int>& cevt = evt;
            AssertThat(int(cevt.end() - cevt.begin()), 6);
            AssertThat(int(evt.end() - evt.begin()), 6);
        }

        struct CopyCounter
        {
            int* copies = nullptr;
            explicit CopyCounter(int* counter) noexcept : copies{counter} {}
            CopyCounter(const CopyCounter& c) noexcept : copies{c.copies} { ++*copies; }
            CopyCounter(CopyCounter&& c) noexcept = default;
            CopyCounter& operator=(const CopyCounter&) = default;
            CopyCounter& operator=(CopyCounter&&) noexcept = default;
            ~CopyCounter() = default;
        };

        TestCase(multicast_delegate_copies_a_by_value_argument_once_per_listener)
        {
            int copies = 0;
            multicast_delegate<CopyCounter> evt;
            evt += [](CopyCounter c) { (void)c; };
            evt += [](CopyCounter c) { (void)c; };
            CopyCounter counter { &copies };
            evt(counter);
            AssertThat(copies, 2);

            copies = 0;
            multicast_delegate<const CopyCounter&> by_ref;
            by_ref += [](const CopyCounter& c) { (void)c; };
            by_ref.invoke(counter);
            AssertThat(copies, 0);
        }

        TestCase(multicast_delegate_empty_states)
        {
            multicast_delegate<int> evt;
            const multicast_delegate<int>& cevt = evt;
            AssertThat(evt.good(), false);
            AssertThat(evt.empty(), true);
            AssertThat(bool(evt), false);
            AssertThat(cevt.begin() == cevt.end(), true);
            AssertThat(evt.begin() == evt.end(), true);
            evt(1); // no container, so nothing runs
            evt.invoke(1);
            evt.remove(&event_func_int);

            evt += &event_func_int;
            AssertThat(bool(evt), true);
            evt -= &event_func_int;
            AssertThat(evt.empty(), true); // the container stays, but holds nothing
            AssertThat(evt.good(), false);
            AssertThat(bool(evt), false);
            evt(1);

            evt += &event_func_int;
            multicast_delegate<int>& alias = evt;
            evt = alias;
            AssertThat(evt.size(), 1);
            evt.clear();
            AssertThat(evt.size(), 0);
        }

        ////////////////////////////////////////////////////
    };
    
    // NOLINTEND(performance-*,readability-make-member-function-const,bugprone-exception-escape)
}
