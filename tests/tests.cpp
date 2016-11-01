#include "tests.h"
#include <cstdlib>
#include <typeinfo>
#include <algorithm>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#include <strview.h>

int test::asserts_failed = 0;
static vector<test*> g_tests;



test::test(const char* name) : name(name)
{
    g_tests.push_back(this);
}
test::~test()
{
    if (!g_tests.empty())
    {
        auto it = find(g_tests.begin(), g_tests.end(), this);
        if (it != g_tests.end())
            g_tests.erase(it);
    }
}
void test::assert_failed(const char* file, int line, const char* expression, ...)
{
    static const char path[] = __FILE__;
    static int path_skip = sizeof(path) - 10;

    ++asserts_failed;
    printf("FAILURE %12s:%d    %s\n", file + path_skip, line, expression);
}

void test::run_test()
{
    char title[256];
    int len = sprintf(title, "--------  running '%s'  --------", name);
    printf("%s\n", title);
    run();
    for (auto& testFunc : test_funcs) {
        testFunc();
    }
    printf("%s\n\n", (char*)memset(title, '-', len));
}

int test::add_test_func(test_func && func) 
{
    test_funcs.emplace_back(move(func));
    return (int)test_funcs.size() - 1;
}

void test::sleep(int millis)
{
#ifdef _WIN32
    Sleep(millis);
#else
    usleep(millis * 1000);
#endif
}

static void pause()
{
#ifdef _WIN32
    system("pause");
#else
    // no pause on OSX
#endif
}

//TestImpl(test_test)
//{
//	Implement(test_test)
//	{
//		Assert(1 == 1);
//	}
//}
//Instance;


int main(int argc, char* argv[])
{
    int numTest = 0;
    if (argc > 1)
    {
        // if arg is provided, we assume it is either:
        // test_testname or testname
        rpp::strview arg = argv[1];
        const bool exactMatch = arg.starts_with("test_");
        if (exactMatch) printf("Filtering exact match '%s'\n\n", arg.str);
        else            printf("Filtering substr match '%s'\n\n", arg.str);

        for (test* t : g_tests) 
        {
            if ((exactMatch && t->name == arg) ||
                (!exactMatch && strstr(t->name, arg.str)) ) 
            {
                t->run_test();
                ++numTest;
                break;
            }
        }
    }
    else
    {
        printf("Running all tests\n");
        for (test* t : g_tests) {
            t->run_test();
            ++numTest;
        }
    }
    
    if (test::asserts_failed)
    {
        fprintf(stderr, "\nWARNING: %d assertions failed!\n", test::asserts_failed);
        pause();
        return -1;
    }

    if (numTest > 0)  printf("\nSUCCESS: All test runs passed!\n");
    else              printf("\nNOTE: No tests were run! (out of %d)\n", (int)g_tests.size());
    return 0;
}