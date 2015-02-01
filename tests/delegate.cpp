#include "tests.h"
#include <delegate.h>

static int func1(int arg0)
{
	printf("%d: %s\n", arg0, __FUNCTION__);
	return 1;
}
struct BaseClass
{
	int func2(int arg0)
	{
		printf("%d: %s\n", arg0, __FUNCTION__);
		return 2;
	}
	virtual int func3(int arg0)
	{
		printf("%d: %s\n", arg0, __FUNCTION__);
		return 3;
	}
};
struct MyClass : BaseClass
{
	virtual int func3(int arg0) override
	{
		printf("%d: %s\n", arg0, __FUNCTION__);
		return 4;
	}
};
struct MyFunctor
{
	int a = 11, b = 22, c = 33;

	int operator()(int arg0)
	{
		printf("%d: %s\n", arg0, __FUNCTION__);
		return 7;
	}
};

void evt_func1(int arg0)
{
	printf("%d: %s\n", arg0, __FUNCTION__);
}
struct EvtClass
{
	void evt_func2(int arg0)
	{
		printf("%d: %s\n", arg0, __FUNCTION__);
	}
	void evt_func3(int arg0)
	{
		printf("%d: %s\n", arg0, __FUNCTION__);
	}
};

TestImpl(delegate_test)
{
	Implement(delegate_test)
	{
		MyClass inst;
		int params1 = 11, params2 = 22;

		delegate<int(int)> d1 = &func1;
		Assert(d1(10) == 1);
		delegate<int(int)> d2(inst, &MyClass::func2);
		Assert(d2(20) == 2);

		BaseClass binst = inst;
		delegate<int(int)> d3(binst, &BaseClass::func3);
		Assert(d3(30) == 3); // this must result in BaseClass::func3 virtual call

		delegate<int(int)> d4((BaseClass*)&inst, &BaseClass::func3);
		Assert(d4(40) == 4); // this must result in MyClass::func3 virtual call

		delegate<int(int)> d5(inst, &MyClass::func3);
		Assert(d5(50) == 4); // this must result in MyClass::func3 virtual call

		delegate<int(int)> d6 = [](int arg0) {
			printf("%d: %s\n", arg0, __FUNCTION__);
			return 5;
		};
		Assert(d6(60) == 5);
		delegate<int(int)> d7 = [&inst, &params1, &params2](int arg0) {
			printf("%d: %s\n", arg0, __FUNCTION__);
			return 6;
		};
		Assert(d7(70) == 6);

		delegate<int(int)> d8 = MyFunctor();
		Assert(d8(80) == 7);


		EvtClass evc;
		event<void(int)> evt;
		Assert(evt.size() == 0); // yeah...

		// add 2 events
		evt += &evt_func1;
		evt.add(evc, &EvtClass::evt_func2);
		evt(10);
		Assert(evt.size() == 2);

		// remove one event
		evt -= &evt_func1;
		evt(20);
		Assert(evt.size() == 1);

		// try to remove an incorrect function:
		evt -= &func1;
		Assert(evt.size() == 1); // nothing must change
		evt.remove(evc, &EvtClass::evt_func3);
		Assert(evt.size() == 1); // nothing must change

		// remove final event
		evt.remove(evc, &EvtClass::evt_func2);
		evt(00);
		Assert(evt.size() == 0); // must be empty now

	}
} Impl;