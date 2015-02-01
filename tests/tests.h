#pragma once
#include <stdio.h> // some basic printf etc.
#include <vector>

using namespace std;

struct test
{
	static int asserts_failed;
	const char* name;

	test(const char* name);
	virtual ~test();
	void assert_failed(const char* file, int line, const char* expression);
	void run_test();

	virtual void run() = 0;
};

#define Assert(expr) if (!(expr)) assert_failed(__FILE__, __LINE__, #expr)
#define TestImpl(testclass) static struct testclass : public test
#define Implement(testclass) testclass() : test(#testclass){} void run()