#include "tests.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_set>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN 1
    #include <Windows.h>
    #include <conio.h> // _kbhit
#elif __ANDROID__
    #include <unistd.h> // usleep
    #include <android/log.h>
    #include <stdarg.h>
#else
    #include <unistd.h>
    #include <termios.h>
#endif


namespace rpp
{
    using std::mutex;
    using std::unique_lock;
    using std::defer_lock;
    ///////////////////////////////////////////////////////////////////////////

    int test::asserts_failed;

    vector<test_info>& get_rpp_tests()
    {
        if (!_rpp_tests)
            _rpp_tests = new vector<test_info>();
        return *_rpp_tests;
    }

    static vector<test_info>& all_tests = get_rpp_tests();

    void register_test(strview name, test_factory factory, bool autorun)
    {
        get_rpp_tests().emplace_back(test_info{ name, factory, {}, true, autorun });
    }

    ///////////////////////////////////////////////////////////////////////////

    test::test(strview name) : name{name}
    {
    }

    test::~test() = default;

    void test::consolef(ConsoleColor color, const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
    #if _WIN32
        static HANDLE winstd = GetStdHandle(STD_OUTPUT_HANDLE);
        static HANDLE winerr = GetStdHandle(STD_ERROR_HANDLE);
        static const WORD colormap[] = {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, // Default
            FOREGROUND_GREEN, // dark green
            FOREGROUND_RED | FOREGROUND_GREEN, // dark yellow
            FOREGROUND_RED, // dark red
        };

        char buffer[8096];
        int len = vsnprintf(buffer, sizeof(buffer), fmt, ap);
        if (len < 0 || len >= 8096) {
            len = sizeof(buffer) - 1;
            buffer[len] = '\0';
        }

        HANDLE winout = color == Red ? winerr : winstd;
        FILE*  cout   = color == Red ? stderr : stdout;

        static mutex consoleSync;
        {
            unique_lock<mutex> guard{ consoleSync, defer_lock };
            if (consoleSync.native_handle()) guard.lock(); // lock if mutex not destroyed

            SetConsoleTextAttribute(winout, colormap[color]);
            fwrite(buffer, size_t(len), 1, cout); // fwrite to sync with unix-like shells
            SetConsoleTextAttribute(winout, colormap[Default]);
        }
        fflush(cout); // flush needed for proper sync with unix-like shells
    #elif __ANDROID__
        int priority = 0;
        switch (color) {
            case Default: priority = ANDROID_LOG_DEBUG; break;
            case Green:   priority = ANDROID_LOG_INFO;  break;
            case Yellow:  priority = ANDROID_LOG_WARN;  break;
            case Red:     priority = ANDROID_LOG_ERROR; break;
        }
        __android_log_vprint(priority, "rpp", fmt, ap);
    #elif __linux
        FILE* cout = color == Red ? stderr : stdout;
        static const char* colors[] = {
                "\33[0m",  // clears all formatting
                "\33[32m", // green
                "\33[33m", // yellow
                "\33[31m", // red
        };
        static mutex consoleSync;
        {
            unique_lock<mutex> guard{ consoleSync, defer_lock };
            if (consoleSync.native_handle()) guard.lock(); // lock if mutex not destroyed

            fwrite(colors[color], strlen(colors[color]), 1, cout);
            vfprintf(cout, fmt, ap);
            fwrite(colors[Default], strlen(colors[Default]), 1, cout);
        }
        fflush(cout); // flush needed for proper sync with different shells
    #else
        FILE* cout = color == Red ? stderr : stdout;
        vfprintf(cout, fmt, ap);
        fflush(cout); // flush needed for proper sync with different shells
    #endif
    }

    void test::assert_failed(const char* file, int line, const char* fmt, ...)
    {
        const char* filename = file + int(strview{ file }.rfindany("\\/") - file) + 1;

        char message[8192];
        va_list ap; va_start(ap, fmt);
        vsnprintf(message, 8192, fmt, ap);

        ++asserts_failed;
        consolef(Red, "FAILED ASSERTION %12s:%d    %s\n", filename, line, message);
    }

    void test::run_test(strview methodFilter)
    {
        char title[256];
        int len = methodFilter
            ? snprintf(title, sizeof(title), "--------  running '%s.%.*s'  --------", name.str, methodFilter.len, methodFilter.str)
            : snprintf(title, sizeof(title), "--------  running '%s'  --------", name.str);

        consolef(Yellow, "%s\n", title);
        run_init();

        if (methodFilter)
        {
            for (int i = 0; i < test_count; ++i)
                if (test_funcs[i].name.find(methodFilter)) {
                    consolef(Yellow, "%s::%s\n", name.str, test_funcs[i].name.str);
                    run_test(test_funcs[i]);
                }
        }
        else
        {
            for (int i = 0; i < test_count; ++i) {
                if (test_funcs[i].autorun) {
                    consolef(Yellow, "%s::%s\n", name.str, test_funcs[i].name.str);
                    run_test(test_funcs[i]);
                }
            }
        }

        run_cleanup();
        consolef(Yellow, "%s\n\n", (char*)memset(title, '-', (size_t)len)); // "-------------"
    }

    bool test::run_init()
    {
        try
        {
            init_test();
            return true;
        }
        catch (const std::exception& e)
        {
            consolef(Red, "FAILED with EXCEPTION in [%s]::TestInit(): %s\n", name.str, e.what());
            ++asserts_failed;
            return false;
        }
    }

    void test::run_cleanup()
    {
        try
        {
            cleanup_test();
        }
        catch (const std::exception& e)
        {
            consolef(Red, "FAILED with EXCEPTION in [%s]::TestCleanup(): %s\n", name.str, e.what());
            ++asserts_failed;
        }
    }

    void test::run_test(test_func& test)
    {
        try
        {
            (test.lambda.*test.func)();
            if (test.expectedExType) // we expected an exception, but none happened?!
            {
                consolef(Red, "FAILED with expected EXCEPTION NOT THROWN in %s::%s\n", 
                         name.str, test.name.str);
                ++asserts_failed;
            }
        }
        catch (const std::exception& e)
        {
            if (test.expectedExType)
            {
                size_t hash = typeid(e).hash_code();
                if (test.expectedExType == hash)
                {
                    consolef(Yellow, "Caught Expected Exception in %s::%s:\n  %s\n", 
                             name.str, test.name.str, e.what());
                    return;
                }
            }
            consolef(Red, "FAILED with EXCEPTION in %s::%s:\n  %s\n", 
                     name.str, test.name.str, e.what());
            ++asserts_failed;
        }
    }

    void test::sleep(int millis)
    {
        #if _WIN32
            Sleep(millis);
        #elif __ANDROID__
            usleep(useconds_t(millis) * 1000);
        #else
            usleep(millis * 1000);
        #endif
    }

    int test::run_tests(const char* testNamePattern)
    {
        return run_tests(&testNamePattern, 1);
    }

    int test::run_tests(const vector<string>& testNamePatterns)
    {
        vector<const char*> names;
        names.push_back("");
        for (const string& name : testNamePatterns) names.push_back(name.c_str());
        return run_tests((int)names.size(), (char**)names.data());
    }

    int test::run_tests(const char** testNamePatterns, int numPatterns)
    {
        vector<const char*> names;
        names.push_back("");
        names.insert(names.end(), testNamePatterns, testNamePatterns+numPatterns);
        return run_tests((int)names.size(), (char**)names.data());
    }



    int test::run_tests()
    {
        char empty[1] = "";
        char* argv[1] = { empty };
        return run_tests(1, argv);
    }

    int test::add_test_func(test_func func) // @note Because we can't dllexport std::vector
    {
        if (test_count == test_cap)
        {
            test_cap = test_funcs ? test_count * 2 : 8;
            auto* funcs = new test_func[test_cap];
            for (int i = 0; i < test_count; ++i) funcs[i] = test_funcs[i];
            delete[] test_funcs;
            test_funcs = funcs;
        }
        test_funcs[test_count++] = func;
        return test_count - 1;
    }

#if _WIN32 && _MSC_VER
    static void pause(int millis = -1/*forever*/)
    {
        if (!IsDebuggerPresent()) { // only pause if we launched from Visual Studio
            return;
        }
        printf("\nPress any key to continue...\n");

        using namespace std::chrono;
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
    static void move_console_window()
    {
        // move console window to the other monitor to make test debugging more seamless
        // if debugger is attached with Visual Studio
        if (IsDebuggerPresent() && GetSystemMetrics(SM_CMONITORS) > 1)
        {
            vector<HMONITOR> mon;
            EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, RECT*, LPARAM data) {
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
            SetWindowPos(GetConsoleWindow(), nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
    }
#endif

    int test::run_tests(int argc, char* argv[])
    {
    #if _WIN32 && _MSC_VER
        #if _CRTDBG_MAP_ALLOC  
            _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
            _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
        #endif
        move_console_window();
    #endif

        for (test_info& t : all_tests) { // set the defaults
            if (!t.auto_run) t.test_enabled = false;
        }

        int numTest = 0;
        if (argc > 1)
        {
            // if arg is provided, we assume they are:
            // test_testname or testname or -test_testname or -testname
            // OR to run a specific test:  testname.specifictest
            std::unordered_set<strview> enabled, disabled;

            for (int iarg = 1; iarg < argc; ++iarg)
            {
                rpp::strview arg = argv[iarg];
                rpp::strview testName = arg.next('.');
                rpp::strview specific = arg.next('.');

                bool enableTest = testName[0] != '-';
                if (!enableTest) {
                    testName.chomp_first();
                }
                else {
                    enableTest = !testName.starts_with("no:");
                    if (!enableTest) testName.skip(3);
                }

                const bool exactMatch = testName.starts_with("test_");
                if (exactMatch) consolef(Yellow, "Filtering exact tests '%s'\n\n", argv[iarg]);
                else            consolef(Yellow, "Filtering substr tests '%s'\n\n", argv[iarg]);
                
                for (test_info& t : all_tests)
                {
                    if (( exactMatch && t.name == testName) ||
                        (!exactMatch && t.name.find(testName)))
                    {
                        t.case_filter = specific;
                        if (enableTest) enabled.insert(t.name);
                        else            disabled.insert(t.name);
                        break;
                    }
                }
            }

            if (!disabled.empty())
            {
                for (test_info& t : all_tests) {
                    if (t.auto_run) { // only consider disabling auto_run tests
                        t.test_enabled = disabled.find(t.name) == disabled.end();
                        if (!t.test_enabled)
                            consolef(Red, "  Disabled %s\n", t.name.to_cstr());
                    }
                }
            }
            else if (!enabled.empty())
            {
                for (test_info& t : all_tests) { // enable whatever was requested
                    t.test_enabled = enabled.find(t.name) != enabled.end();
                    if (t.test_enabled)
                        consolef(Green, "  Enabled %s\n", t.name.to_cstr());
                }
            }
        }
        else
        {
            consolef(Green, "Running all auto-run tests\n");
            for (test_info& t : all_tests)
                if (!t.auto_run && !t.test_enabled)
                    consolef(Yellow, "  Disabled NoAutoRun %s\n", t.name.to_cstr());
        }

        // run all the marked tests
        for (test_info& t : all_tests) {
            if (t.test_enabled) {
                auto test = t.factory(t.name);
                test->run_test(t.case_filter);
                ++numTest;
            }
        }

        int result = 0;
        if (asserts_failed)
        {
            consolef(Red, "\nWARNING: %d assertions failed!\n", asserts_failed);
            result = -1;
        }
        else if (numTest > 0)
        {
            consolef(Green, "\nSUCCESS: All test runs passed!\n");
        }
        else
        {
            consolef(Yellow, "\nNOTE: No tests were run! (out of %d)\n", (int)all_tests.size());
        }

        #if _WIN32 && _MSC_VER
            #if _CRTDBG_MAP_ALLOC
                _CrtDumpMemoryLeaks();
            #endif
            pause();
        #endif
        return result;
    }

} // namespace rpp

#if RPP_TESTS_DEFINE_MAIN
int main(int argc, char* argv[])
{
    return rpp::test::run_tests(argc, argv);
}
#endif
