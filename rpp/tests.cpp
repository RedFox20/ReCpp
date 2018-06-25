#include "tests.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <cstdarg>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN 1
    #include <Windows.h>
    #include <conio.h> // _kbhit
#elif __ANDROID__
    #include <unistd.h> // usleep
    #include <android/log.h>
#else
    #include <unistd.h>
    #include <termios.h>
#endif
#if __APPLE__
    #include <TargetConditionals.h>
#endif


namespace rpp
{
    using std::mutex;
    using std::unique_lock;
    using std::defer_lock;
    using std::unordered_set;
    ///////////////////////////////////////////////////////////////////////////

#if !RPP_HAS_CXX17
    vector<test_info>* _rpp_tests;
#endif

    vector<test_info>& get_rpp_tests()
    {
        if (!_rpp_tests)
            _rpp_tests = new vector<test_info>();
        return *_rpp_tests;
    }

    static int total_asserts_failed;
    static vector<test_info>& all_tests = get_rpp_tests();

    void register_test(strview name, test_factory factory, bool autorun)
    {
        get_rpp_tests().emplace_back(test_info{ name, factory, {}, true, autorun });
    }

    ///////////////////////////////////////////////////////////////////////////

    struct test_results
    {
        int tests_run = 0;
        int tests_failed = 0;
        vector<string> failures;
    };

    ///////////////////////////////////////////////////////////////////////////

    test::test(strview name) : name{ name } {}
    test::~test() = default;

    enum ConsoleColor { Default, Green, Yellow, Red, };

    static void consolef(ConsoleColor color, const char* fmt, ...)
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
        FILE*  cout = color == Red ? stderr : stdout;

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
#elif __linux || TARGET_OS_OSX
        FILE* cout = color == Red ? stderr : stdout;
        if (isatty(fileno(cout))) // is terminal?
        {
            static const char* colors[] = {
                "\x1b[0m",  // clears all formatting
                "\x1b[32m", // green
                "\x1b[33m", // yellow
                "\x1b[31m", // red
            };
            static mutex consoleSync;
            {
                unique_lock<mutex> guard{ consoleSync, defer_lock };
                if (consoleSync.native_handle()) guard.lock(); // lock if mutex not destroyed

                fwrite(colors[color], strlen(colors[color]), 1, cout);
                vfprintf(cout, fmt, ap);
                fwrite(colors[Default], strlen(colors[Default]), 1, cout);
            }
        }
        else // it's a file descriptor
        {
            vfprintf(cout, fmt, ap);
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

        ++total_asserts_failed;
        consolef(Red, "FAILED ASSERTION %12s:%d    %s\n", filename, line, message);
    }

    void test::assert_failed_custom(const char* fmt, ...)
    {
        char message[8192];
        va_list ap; va_start(ap, fmt);
        vsnprintf(message, 8192, fmt, ap);

        ++total_asserts_failed;
        consolef(Red, message);
    }

    bool test::run_test(strview methodFilter)
    {
        char title[256];
        int len = methodFilter
            ? snprintf(title, sizeof(title), "--------  running '%s.%.*s'  --------", name.str, methodFilter.len, methodFilter.str)
            : snprintf(title, sizeof(title), "--------  running '%s'  --------", name.str);

        consolef(Yellow, "%s\n", title);
        run_init();

        int numTests = 0;
        bool allSuccess = true;
        if (methodFilter)
        {
            for (int i = 0; i < test_count; ++i) {
                if (test_funcs[i].name.find(methodFilter)) {
                    consolef(Yellow, "%s::%s\n", name.str, test_funcs[i].name.str);
                    allSuccess &= run_test(test_funcs[i]);
                    ++numTests;
                }
            }
            if (!numTests) {
                consolef(Yellow, "No tests matching '%.*s' in %s\n", methodFilter.len, methodFilter.str, name.str);
            }
        }
        else
        {
            for (int i = 0; i < test_count; ++i) {
                if (test_funcs[i].autorun) {
                    consolef(Yellow, "%s::%s\n", name.str, test_funcs[i].name.str);
                    allSuccess &= run_test(test_funcs[i]);
                    ++numTests;
                }
            }
            if (!numTests) {
                consolef(Yellow, "No autorun tests discovered in %s\n", name.str);
            }
        }

        run_cleanup();
        consolef(Yellow, "%s\n\n", (char*)memset(title, '-', (size_t)len)); // "-------------"
        return allSuccess && numTests > 0;
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
            assert_failed_custom("FAILED with EXCEPTION in [%s]::TestInit(): %s\n", name.str, e.what());
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
            assert_failed_custom("FAILED with EXCEPTION in [%s]::TestCleanup(): %s\n", name.str, e.what());
        }
    }

    bool test::run_test(test_func& test)
    {
        int before = total_asserts_failed;
        try
        {
            (test.lambda.*test.func)();
            if (test.expectedExType) // we expected an exception, but none happened?!
            {
                assert_failed_custom("FAILED with expected EXCEPTION NOT THROWN in %s::%s\n",
                                     name.str, test.name.str);
            }
        }
        catch (const std::exception& e)
        {
            if (test.expectedExType && test.expectedExType == typeid(e).hash_code())
            {
                consolef(Yellow, "Caught Expected Exception in %s::%s:\n  %s\n",
                    name.str, test.name.str, e.what());
            }
            else
            {
                assert_failed_custom("FAILED with EXCEPTION in %s::%s:\n  %s\n",
                                     name.str, test.name.str, e.what());
            }
        }
        int totalFailures = total_asserts_failed - before;
        return totalFailures <= 0;
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

    int test::run_tests(strview testNamePatterns)
    {
        vector<string> names;
        while (strview pattern = testNamePatterns.next(" \t"))
            names.emplace_back(pattern);
        return run_tests(names);
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
        names.insert(names.end(), testNamePatterns, testNamePatterns + numPatterns);
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
            HMONITOR otherMon = consoleMon != mon[0] ? mon[0] : mon[1];

            MONITORINFO consoleMI = { sizeof(MONITORINFO), {}, {}, 0 };
            MONITORINFO otherMI   = { sizeof(MONITORINFO), {}, {}, 0 };
            GetMonitorInfo(consoleMon, &consoleMI);
            GetMonitorInfo(otherMon,   &otherMI);

            int x = consoleMI.rcMonitor.left > otherMI.rcMonitor.left // moveLeft ?
                ? otherMI.rcMonitor.right - (consoleRect.left - consoleMI.rcMonitor.left) - (consoleRect.right - consoleRect.left)
                : otherMI.rcMonitor.left  + (consoleRect.left - consoleMI.rcMonitor.left);
            int y = otherMI.rcMonitor.top + (consoleRect.top - consoleMI.rcMonitor.top);
            SetWindowPos(GetConsoleWindow(), nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
    }
#endif

    static void set_test_defaults()
    {
        for (test_info& t : all_tests)
        {
            if (!t.auto_run)
                t.test_enabled = false;
        }
    }

    static void enable_disable_tests(unordered_set<strview>& enabled,
                                     unordered_set<strview>& disabled)
    {
        if (enabled.empty() && disabled.empty())
        {
            consolef(Red, "  No matching tests found for provided arguments!\n");
            for (test_info& t : all_tests) // disable all tests to trigger test report warnings
                t.test_enabled = false;
        }
        else if (!disabled.empty())
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
    
    // ignore empty args
    static bool has_filter_args(int argc, char* argv[])
    {
        for (int iarg = 1; iarg < argc; ++iarg) {
            strview arg = strview{argv[iarg]}.trim();
            if (arg) return true;
        }
        return false;
    }

    static void select_tests_from_args(int argc, char* argv[])
    {
        // if arg is provided, we assume they are:
        // test_testname or testname or -test_testname or -testname
        // OR to run a specific test:  testname.specifictest
        std::unordered_set<strview> enabled, disabled;
        for (int iarg = 1; iarg < argc; ++iarg)
        {
            strview arg = strview{argv[iarg]}.trim();
            if (!arg) continue;
            
            strview testName = arg.next('.');
            strview specific = arg.next('.');

            bool enableTest = testName[0] != '-';
            if (!enableTest) {
                testName.chomp_first();
            }
            else {
                enableTest = !testName.starts_with("no:");
                if (!enableTest) testName.skip(3);
            }

            const bool exactMatch = testName.starts_with("test_");
            if (exactMatch) consolef(Yellow, "Filtering exact  tests '%s'\n", argv[iarg]);
            else            consolef(Yellow, "Filtering substr tests '%s'\n", argv[iarg]);

            bool match = false;
            for (test_info& t : all_tests)
            {
                if ((exactMatch && t.name == testName) ||
                    (!exactMatch && t.name.find(testName)))
                {
                    t.case_filter = specific;
                    if (enableTest) enabled.insert(t.name);
                    else            disabled.insert(t.name);
                    match = true;
                    break;
                }
            }
            if (!match) {
                consolef(Red, "  No matching test for '%.*s'\n", testName.len, testName.str);
            }
        }
        enable_disable_tests(enabled, disabled);
    }

    static void enable_all_autorun_tests()
    {
        consolef(Green, "Running all AutoRun tests\n");
        for (test_info& t : all_tests)
            if (!t.auto_run && !t.test_enabled)
                consolef(Yellow, "  Disabled NoAutoRun %s\n", t.name.to_cstr());
    }

    test_results run_all_marked_tests()
    {
        test_results results;
        for (test_info& t : all_tests)
        {
            if (t.test_enabled)
            {
                auto test = t.factory(t.name);
                if (!test->run_test(t.case_filter))
                {
                    ++results.tests_failed;
                }
                ++results.tests_run;
            }
        }
        return results;
    }

    static int print_final_summary(const test_results& results)
    {
        int numTests = results.tests_run;
        int failed = (int)total_asserts_failed;
        if (failed > 0)
        {
            if (numTests == 1)
                consolef(Red, "\nWARNING: Test failed with %d assertions!\n", failed);
            else
                consolef(Red, "\nWARNING: %d/%d tests failed with %d assertions!\n",
                               results.tests_failed, numTests, failed);
            return -1;
        }
        if (numTests <= 0)
        {
            consolef(Yellow, "\nNOTE: No tests were run! (out of %d available)\n", (int)all_tests.size());
            return -1;
        }
        if (numTests == 1) consolef(Green, "\nSUCCESS: Test passed!\n");
        else               consolef(Green, "\nSUCCESS: All %d tests passed!\n", numTests);
        return 0;
    }

    int test::run_tests(int argc, char* argv[])
    {
    #if _WIN32 && _MSC_VER
        #if _CRTDBG_MAP_ALLOC  
            _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF);
            _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
        #endif
        move_console_window();
    #endif

        set_test_defaults();
        if (has_filter_args(argc, argv))
            select_tests_from_args(argc, argv);
        else
            enable_all_autorun_tests();

        test_results testResults = run_all_marked_tests();
        int returnCode = print_final_summary(testResults);
        #if _WIN32 && _MSC_VER
            #if _CRTDBG_MAP_ALLOC
                _CrtDumpMemoryLeaks();
            #endif
            pause();
        #endif
        return returnCode;
    }

} // namespace rpp

#if RPP_TESTS_DEFINE_MAIN
int main(int argc, char* argv[])
{
    return rpp::test::run_tests(argc, argv);
}
#endif
