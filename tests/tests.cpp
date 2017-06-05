#include "tests.h"
#include <cstdlib>
#include <chrono>
#include <memory>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <conio.h> // _kbhit
#else
    #include <unistd.h>
    #include <termios.h>
#endif

namespace rpp
{
    int test::asserts_failed;

    // there are initialization order issues with this global variable, so wrap it to guarantee initialization order
    static vector<test*>& all_tests() noexcept
    {
        static vector<test*> tests;
        return tests;
    }

    test::test(strview name) : name(name)
    {
        all_tests().push_back(this);
    }

    test::~test()
    {
        if (!all_tests().empty())
        {
            auto it = find(all_tests().begin(), all_tests().end(), this);
            if (it != all_tests().end())
                all_tests().erase(it);
        }
    }

    void test::consolef(ConsoleColor color, const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
    #if _WIN32
        static HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        static const int colormap[] = {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, // Default
            FOREGROUND_GREEN, // dark green
            FOREGROUND_RED | FOREGROUND_GREEN, // dark yellow
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

    void test::assert_failed(const char* file, int line, const char* fmt, ...)
    {
        static const char path[] = __FILE__;
        static int path_skip = sizeof(path) - 10;

        char message[8192];
        va_list ap; va_start(ap, fmt);
        vsnprintf(message, 8192, fmt, ap);

        ++asserts_failed;
        consolef(Red, "FAILURE %12s:%d    %s\n", file + path_skip, line, message);
    }

    void test::run_test(strview methodFilter)
    {
        char title[256];
        int len = methodFilter
            ? snprintf(title, sizeof(title), "--------  running '%s.%.*s'  --------", name.str, methodFilter.len, methodFilter.str)
            : snprintf(title, sizeof(title), "--------  running '%s'  --------", name.str);

        consolef(Yellow, "%s\n", title);
        run();

        auto funcs = test_funcs.data();
        auto count = test_funcs.size();
        if (methodFilter)
        {
            for (auto i = 0u; i < count; ++i) {
                if (funcs[i].name.find(methodFilter)) {
                    (funcs[i].lambda.*funcs[i].func)();
                }
            }
        }
        else
        {
            for (auto i = 0u; i < count; ++i) {
                (funcs[i].lambda.*funcs[i].func)();
            }
        }
        consolef(Yellow, "%s\n\n", (char*)memset(title, '-', len)); // "-------------"
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
    static int _kbhit()
    {
        termios oldSettings; tcgetattr(STDIN_FILENO, &oldSettings);
        termios newSettings = oldSettings;
        newSettings.c_cc[VTIME] = 0;
        newSettings.c_cc[VMIN] = 0;
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
        while (!_kbhit())
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

    int test::run_tests(const char* testNamePattern)
    {
        char empty[1] = "";
        char name[256]; strncpy(name, testNamePattern, 256);
        char* argv[2] = { empty, name };
        return run_tests(1, argv);
    }

    int test::run_tests()
    {
        char empty[1] = "";
        char* argv[1] = { empty };
        return run_tests(1, argv);
    }

    int test::run_tests(int argc, char* argv[])
    {
        // move console window to the other monitor to make test debugging more seamless
        // if debugger is attached with Visual Studio
    #if _WIN32 && _MSC_VER
        int numMonitors = 0;
        if (IsDebuggerPresent() && (numMonitors = GetSystemMetrics(SM_CMONITORS)) > 1)
        {
            vector<HMONITOR> mon;
            EnumDisplayMonitors(0, 0, [](HMONITOR monitor, HDC, RECT*, LPARAM data) {
                ((vector<HMONITOR>*)data)->push_back(monitor); return 1; }, (LPARAM)&mon);

            RECT consoleRect; GetWindowRect(GetConsoleWindow(), &consoleRect);
            HMONITOR consoleMon = MonitorFromRect(&consoleRect, MONITOR_DEFAULTTONEAREST);
            HMONITOR otherMon   = consoleMon != mon[0] ? mon[0] : mon[1];

            MONITORINFO consoleMI = { sizeof(MONITORINFO) };
            MONITORINFO otherMI   = { sizeof(MONITORINFO) };
            GetMonitorInfo(consoleMon, &consoleMI);
            GetMonitorInfo(otherMon, &otherMI);

            int x = consoleMI.rcMonitor.left > otherMI.rcMonitor.left // moveLeft ?
                ? otherMI.rcMonitor.right - (consoleRect.left - consoleMI.rcMonitor.left) - (consoleRect.right-consoleRect.left)
                : otherMI.rcMonitor.left  + (consoleRect.left - consoleMI.rcMonitor.left);
            int y = otherMI.rcMonitor.top + (consoleRect.top - consoleMI.rcMonitor.top);
            SetWindowPos(GetConsoleWindow(), 0, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
    #endif

        int numTest = 0;
        if (argc > 1)
        {
            // if arg is provided, we assume it is either:
            // test_testname or testname
            // OR to run a specific test:  testname.specifictest

            for (int iarg = 1; iarg < argc; ++iarg)
            {
                rpp::strview arg = argv[iarg];
                rpp::strview testName = arg.next('.');
                rpp::strview specific = arg.next('.');

                const bool exactMatch = testName.starts_with("test_");
                if (exactMatch) consolef(Yellow, "Filtering exact tests '%s'\n\n", argv[iarg]);
                else            consolef(Yellow, "Filtering substr tests '%s'\n\n", argv[iarg]);

                for (test* t : all_tests())
                {
                    if ((exactMatch && t->name == testName) ||
                        (!exactMatch && t->name.find(testName)))
                    {
                        t->run_test(specific);
                        ++numTest;
                        break;
                    }
                }
            }
        }
        else
        {
            consolef(Green, "Running all tests\n");
            for (test* t : all_tests()) {
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
            consolef(Green, "\nSUCCESS: All test runs passed!\n");
        else
            consolef(Yellow, "\nNOTE: No tests were run! (out of %d)\n", (int)all_tests().size());
        pause(5000);
        return 0;
    }
}

#if RPP_TESTS_DEFINE_MAIN
int main(int argc, char* argv[])
{
    return rpp::test::run_tests(argc, argv);
}
#endif
