#include "tests.h"
#include <cstdlib>
#include <chrono>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <conio.h>
#else
    #include <unistd.h>
    #include <termios.h>
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

enum ConsoleColor { Default, Green, Yellow, Red, };

static void consolef(ConsoleColor color, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#if _WIN32
    static HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    static const int colormap[] = {
        FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE, // Default
        FOREGROUND_GREEN, // dark green
        FOREGROUND_RED|FOREGROUND_GREEN, // dark yellow
        FOREGROUND_RED, // dark red
    };
    if (color != Default) SetConsoleTextAttribute(console, colormap[color]);
    if (color == Red) vfprintf(stderr, fmt, ap);
    else              vfprintf(stdout, fmt, ap);
    if (color != Default) SetConsoleTextAttribute(console, colormap[Default]);
#else // @todo Proper Linux & OSX implementations
    if (color == Red) vfprintf(stderr, fmt, ap);
    else              vfprintf(stdout, fmt, ap);
#endif
}

void test::assert_failed(const char* file, int line, const char* expression, ...)
{
    static const char path[] = __FILE__;
    static int path_skip = sizeof(path) - 10;

    ++asserts_failed;
    consolef(Red, "FAILURE %12s:%d    %s\n", file + path_skip, line, expression);
}

void test::run_test()
{
    char title[256];
    int len = sprintf(title, "--------  running '%s'  --------", name);
    consolef(Yellow, "%s\n", title);
    run();
    for (auto& testFunc : test_funcs) {
        testFunc();
    }
    consolef(Yellow, "%s\n\n", (char*)memset(title, '-', len)); // "-------------"
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

#ifndef _WIN32 // Linux:
static int kbhit()
{
    termios oldSettings; tcgetattr(STDIN_FILENO, &oldSettings);
    termios newSettings = oldSettings;
    newSettings.c_cc[VTIME] = 0;
    newSettings.c_cc[VMIN]  = 0;
    newSettings.c_iflag &= ~(IXOFF);
    newSettings.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);

    char keycodes[16];
    int count = read(stdin, (void*)keycodes, 16);

    tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);

    return count == 0 ? 0 : keycodes[0];
}
#endif

static void pause(int millis = -1/*forever*/)
{
    printf("\nPress any key to continue...");

    using namespace chrono;
    auto start = system_clock::now();
    while (!kbhit())
    {
        if (millis != -1)
        {
            auto elapsed = duration_cast<milliseconds>(system_clock::now() - start);
            if (elapsed.count() >= millis)
                break;
        }
        test::sleep(50);
    }
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
        if (exactMatch) consolef(Yellow, "Filtering exact tests '%s'\n\n", arg.str);
        else            consolef(Yellow, "Filtering substr tests '%s'\n\n", arg.str);

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
        consolef(Green, "Running all tests\n");
        for (test* t : g_tests) {
            t->run_test();
            ++numTest;
        }
    }
    
    if (test::asserts_failed)
    {
        consolef(Red, "\nWARNING: %d assertions failed!\n", test::asserts_failed);
        pause();
        return -1;
    }

    if (numTest > 0)
    {
        consolef(Green, "\nSUCCESS: All test runs passed!\n");
        pause(3000);
    }
    else
    {
        consolef(Yellow, "\nNOTE: No tests were run! (out of %d)\n", (int)g_tests.size());
        pause(5000);
    }
    return 0;
}