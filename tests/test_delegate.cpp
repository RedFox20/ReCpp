#include <rpp/delegate.h>
#include <rpp/tests.h>
using namespace rpp;


// a generic data container for testing instances, functors and lambdas
class Data
{
public:
    char* data = nullptr;
    static char* alloc(const char* str) {
        size_t n = strlen(str) + 1;
        return (char*)memcpy(new char[n], str, n);
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

static Data validate(const char* name, const Data& a)
{
    printf("%s: '%s'\n", name, a.data);
    return Data{name};
}
static Data validate(const char* name, const Data& a, const Data& x)
{
    printf("%s: '%s' '%s'\n", name, a.data, x.data);
    return Data{name};
}


TestImpl(test_delegate)
{
    using DataDelegate = delegate<Data(Data a)>;
    Data data;

    TestInit(test_delegate)
    {
        //#if __GNUG__
        //		BaseClass a;
        //		typedef int(*func_type)(BaseClass* a, int i);
        //		int (BaseClass::*membfunc)(int i) = &BaseClass::func3;
        //		func_type fp = (func_type)(&a->*membfunc);
        //		printf("BaseClass::func3 %p\n", fp);
        //#endif
    }

    ////////////////////////////////////////////////////

    TestCase(functions)
    {
        Data (*function)(Data a) = [](Data a)
        {
            return validate("function", a);
        };

        DataDelegate func = function;
        AssertThat(func(data), "function");
    }

    ////////////////////////////////////////////////////

    struct Base
    {
        Data x;
        virtual ~Base(){}
        Data method(Data a)
        {
            return validate("method", a, x);
        }
        Data const_method(Data a) const
        {
            return validate("const_method", a, x);
        }
        virtual Data virtual_method(Data a)
        {
            return validate("virtual_method", a, x);
        }
    };
    struct Derived : Base
    {
        Data virtual_method(Data a) override
        {
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
        u.dfunc = (dummy_type)method;

        dummy* dum = (dummy*)obj;
        (dum->*u.dfunc)(data);
    }

    TestCase(methods)
    {
        Derived inst;
        DataDelegate func1(inst, &Derived::method);
        AssertThat(func1(data), "method");

        DataDelegate func2(inst, &Derived::const_method);
        AssertThat(func2(data), "const_method");
    }

    TestCase(virtuals)
    {
        Base    base;
        Derived inst;

        DataDelegate func1(base, &Base::virtual_method);
        AssertThat(func1(data), "virtual_method");

        DataDelegate func2((Base*)&inst, &Base::virtual_method);
        AssertThat(func2(data), "derived_method");

        DataDelegate func3(inst, &Derived::virtual_method);
        AssertThat(func3(data), "derived_method");
    }

    ////////////////////////////////////////////////////

    TestCase(lambdas)
    {
        DataDelegate lambda1 = [](Data a)
        {
            return validate("lambda1", a);
        };
        AssertThat(lambda1(data), "lambda1");

        DataDelegate lambda2 = DataDelegate([x=data](Data a)
        {
            return validate("lambda2", a, x);
        });
        AssertThat(lambda2(data), "lambda2");
    }

    TestCase(nested_lambdas)
    {
        DataDelegate lambda = [x=data](Data a)
        {
            DataDelegate nested = [x=x](Data a)
            {
                return validate("nested_lambda", x);
            };
            return nested(a);
        };
        AssertThat(lambda(data), "nested_lambda");

        DataDelegate moved_lambda = move(lambda);
        AssertThat(!lambda, true);
        AssertThat(moved_lambda(data), "nested_lambda");
    }

    TestCase(functor)
    {
        struct Functor
        {
            Data x;
            Data operator()(Data a) const
            {
                return validate("functor", a, x);
            }
        };

        DataDelegate func = Functor();
        AssertThat(func(data), "functor");
    }

    ////////////////////////////////////////////////////

    static void event_func(Data a)
    {
        validate("event_func", a);
    }

    TestCase(multicast_delegates)
    {
        struct Receiver
        {
            Data x;
            void event_method(Data a)
            {
                validate("event_method", a, x);
            }
            void const_method(Data a) const
            {
                validate("const_method", a, x);
            }
            void unused_method(Data a) const { const_method(a); }
        };

        Receiver receiver;
        multicast_delegate<Data> evt;
        Assert(evt.size() == 0); // yeah...

        // add 2 events
        evt += &event_func;
        evt.add(receiver, &Receiver::event_method);
        evt.add(receiver, &Receiver::const_method);
        evt(data);
        Assert(evt.size() == 3);

        // remove one event
        evt -= &event_func;
        evt(data);
        Assert(evt.size() == 2);

        // try to remove an incorrect function:
        evt -= &event_func;
        Assert(evt.size() == 2); // nothing must change
        evt.remove(receiver, &Receiver::unused_method);
        Assert(evt.size() == 2); // nothing must change

        // remove final events
        evt.remove(receiver, &Receiver::event_method);
        evt.remove(receiver, &Receiver::const_method);
        evt(data);
        Assert(evt.size() == 0); // must be empty now
    }

    ////////////////////////////////////////////////////

} Impl;
