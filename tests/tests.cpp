#include "tests.h"
#include <stdlib.h>
#include <stdio.h>
#include <typeinfo>
#include <algorithm>

int test::asserts_failed = 0;
static vector<test*>* g_tests = 0;



test::test(const char* name) : name(name)
{
	static vector<test*> tests;
	g_tests = &tests;
	tests.push_back(this);
}
test::~test()
{
	vector<test*>& tests = *g_tests;
	if (!tests.empty())
	{
		auto it = find(begin(tests), end(tests), this);
		if (it != end(tests))
			tests.erase(it);
	}
}
void test::assert_failed(const char* file, int line, const char* expression)
{
	static const char path[] = __FILE__;
	static int path_skip = sizeof(path) - 10;

	++asserts_failed;
	printf("FAILURE %12s:%d    %s\n", file + path_skip, line, expression);
}

void test::run_test()
{
	printf("running '%s'\n", name);
	run();
}


TestImpl(test_test)
{
	Implement(test_test)
	{
		Assert(1 == 1);
	}
}
Instance;


int main(int argc, char** argv, char** envp)
{
	for (test* t : *g_tests)
		t->run_test();
	
	if (test::asserts_failed)
	{
		printf("\nWARNING: %d assertions failed!\n", test::asserts_failed);
		system("pause");
		return -1;
	}
	system("pause");
	return 0;
}