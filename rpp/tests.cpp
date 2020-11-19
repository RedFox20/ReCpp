#include "tests.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <cstdarg>
#include <cassert>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN 1
#  include <Windows.h>
#  include <conio.h> // _kbhit
#elif __ANDROID__
#  include <unistd.h> // usleep
#  include <android/log.h>
// #  include <linux/memfd.h>
#else
#  include <unistd.h>
#  include <termios.h>
#endif
#if __APPLE__
#  include <TargetConditionals.h>
#endif
#if !_WIN32 // mmap, shm_open
#  include <sys/mman.h>
#  include <fcntl.h>
#endif


namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////


    struct mapped_state
    {
        std::vector<test_info> global_tests;
        TestVerbosity verbosity = TestVerbosity::Summary;
    };

    /**
     * Utility for memory mapping a global rpp::test state,
     * so the tests can be run across dll/so/dylib boundaries.
     */
    template<class T> class shared_memory_view
    {
        struct shared
        {
            T stored_object {};
            int initialized {}; // MMAP will also set this to 0
        };
        shared* shared_mem = nullptr;
    #if _MSC_VER
        HANDLE handle = INVALID_HANDLE_VALUE;
    #else
        int shm_fd = 0;
    #endif
    public:
        T& get()
        {
            if (!shared_mem)
            {
            #if _MSC_VER
                std::string name = "Local\\RppTestsState_"s + rpp::to_string(GetCurrentProcessId());
                handle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
                if (!handle)
                {
                    handle = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                               0, sizeof(shared), name.c_str());
                    if (handle == nullptr)
                    {
                        char error[1024];
                        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr,
                                       GetLastError(), 0, error, 1024, nullptr);
                        fprintf(stderr, "CreateFileMapping failed: %s\n", error);
                        fflush(stderr);
                        std::terminate();
                    }
                }
                shared_mem = (shared*)MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(shared));
            #elif __ANDROID__
                // @todo Properly implement Process-Wide shared variables
                shared_mem = new shared();
                shared_mem->initialized = 1;
                return shared_mem->stored_object;
            #else
                std::string name = "/rpp_tests_state_"s + rpp::to_string(getpid());
                #if __ANDROID__
                    // shm_fd = open(name.c_str(), O_RDWR);
                    // if (shm_fd == -1) {
                    //     shm_fd = memfd_create(name.c_str(), MFD_ALLOW_SEALING | MFD_CLOEXEC);
                    //     if (shm_fd == -1) {
                    //         fprintf(stderr, "memfd_create failed: %s\n", strerror(errno));
                    //         fflush(stderr);
                    //     }
                    // }
                #else
                    shm_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
                    if (shm_fd == -1) fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
                #endif
                assert(shm_fd != -1);
                int fterr = ftruncate(shm_fd, sizeof(shared));
                if (fterr != 0) {
                    fprintf(stderr, "ftruncate(%d,%d) failed: %s\n",
                                    shm_fd, (int)sizeof(shared), strerror(errno));
                    fflush(stderr);
                }
                shared_mem = (shared*)mmap(nullptr, sizeof(shared),
                                      PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
            #endif
                assert(shared_mem != nullptr);
                if (!shared_mem->initialized)
                {
                    shared_mem->initialized = true;
                    new (&shared_mem->stored_object) T();
                }
            }
            return shared_mem->stored_object;
        }
    };


    static mapped_state& state()
    {
        static shared_memory_view<mapped_state> s;
        return s.get();
    }

    void register_test(strview name, test_factory factory, bool autorun)
    {
        state().global_tests.emplace_back(test_info{ name, factory, {}, true, autorun });
    }

    ///////////////////////////////////////////////////////////////////////////

    struct test_failure
    {
        std::string testname; // major test name
        std::string testcase; // failed case name
        std::string message;
        std::string file;
        int line;
    };

    struct test_results
    {
        int tests_run = 0;
        int tests_failed = 0;
        int asserts_failed = 0;
        std::vector<test_failure> failures;
    };

    ///////////////////////////////////////////////////////////////////////////

    test::test(strview name) : name{ name } {}
    test::~test() = default;

    enum ConsoleColor { Default, Green, Yellow, Red, };

    static void console(ConsoleColor color, const char* str, int len)
    {
    #if _WIN32
        static HANDLE winstd = GetStdHandle(STD_OUTPUT_HANDLE);
        static HANDLE winerr = GetStdHandle(STD_ERROR_HANDLE);
        static const WORD colormap[] = {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, // Default
            FOREGROUND_GREEN, // dark green
            FOREGROUND_RED | FOREGROUND_GREEN, // dark yellow
            FOREGROUND_RED, // dark red
        };

        HANDLE winout = color == Red ? winerr : winstd;
        FILE*  cout   = color == Red ? stderr : stdout;

        static std::mutex consoleSync;
        {
            std::unique_lock<std::mutex> guard{ consoleSync, std::defer_lock };
            if (consoleSync.native_handle()) guard.lock(); // lock if mutex not destroyed

            SetConsoleTextAttribute(winout, colormap[color]);
            fwrite(str, size_t(len), 1, cout); // fwrite to sync with unix-like shells
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
        __android_log_write(priority, "rpp", str);
    #elif __linux || TARGET_OS_OSX
        static bool stdoutIsAtty = isatty(fileno(stdout));
        static bool stderrIsAtty = isatty(fileno(stderr));

        FILE* cout = (color == Red) ? stderr : stdout;
        bool isTerminal = (color == Red) ? stderrIsAtty : stdoutIsAtty;

        if (isTerminal)
        {
            static const char* colors[] = {
                "\x1b[0m",  // clears all formatting
                "\x1b[32m", // green
                "\x1b[33m", // yellow
                "\x1b[31m", // red
            };
            static std::mutex consoleSync;
            {
                std::unique_lock<std::mutex> guard{ consoleSync, std::defer_lock };
                if (consoleSync.native_handle()) guard.lock(); // lock if mutex not destroyed

                fwrite(colors[color], strlen(colors[color]), 1, cout);
                fwrite(str, size_t(len), 1, cout);
                fwrite(colors[Default], strlen(colors[Default]), 1, cout);
            }
        }
        else // it's a file descriptor
        {
            fwrite(str, size_t(len), 1, cout);
        }
        fflush(cout); // flush needed for proper sync with different shells
    #else
        FILE* cout = color == Red ? stderr : stdout;
        fwrite(str, size_t(len), 1, cout);
        fflush(cout); // flush needed for proper sync with different shells
    #endif
    }

    #define safe_vsnprintf_msg_len(fmt) \
        char msg[8192]; va_list ap; va_start(ap, fmt); \
        int len = vsnprintf(msg, sizeof(msg), fmt, ap); \
        if (len < 0 || len >= (int)sizeof(msg)) { \
            len = (int)sizeof(msg) - 1; \
            msg[len] = '\0'; \
        }

    static void consolef(ConsoleColor color, const char* fmt, ...)
    {
        safe_vsnprintf_msg_len(fmt);
        console(color, msg, len);
    }

    // only show immediate message if TestLabel-level verbosity
    #define LogTestLabel(expr) \
        if (state().verbosity >= TestVerbosity::TestLabels) { expr; }

    void test::assert_failed(const char* file, int line, const char* fmt, ...)
    {
        const char* filename = file + int(strview{ file }.rfindany("\\/") - file) + 1;
        safe_vsnprintf_msg_len(fmt);
        LogTestLabel(consolef(Red, "FAILED ASSERTION %12s:%d    %s\n", filename, line, msg));

        if (current_results)
            add_assert_failure(filename, line, msg, len);
    }

    void test::assert_failed_custom(const char* fmt, ...)
    {
        safe_vsnprintf_msg_len(fmt);
        LogTestLabel(console(Red, msg, len));

        if (current_results)
            add_assert_failure("", 0, msg, len);
    }

    void test::add_assert_failure(const char* file, int line, const char* msg, int len)
    {
        static std::mutex mtx;
        test_failure fail { name, current_func->name, std::string{msg,msg+len}, std::string{file}, line };
        {
            std::lock_guard<std::mutex> lock {mtx};
            current_results->asserts_failed++;
            current_results->failures.emplace_back(std::move(fail));
        }
    }

    void test::print_error(const char* fmt, ...)
    {
        LogTestLabel( safe_vsnprintf_msg_len(fmt); console(Red, msg, len) );
    }

    void test::print_warning(const char* fmt, ...)
    {
        LogTestLabel( safe_vsnprintf_msg_len(fmt); console(Yellow, msg, len) );
    }

    void test::print_info(const char* fmt, ...)
    {
        LogTestLabel( safe_vsnprintf_msg_len(fmt); console(Default, msg, len) );
    }

    static ConsoleColor SeverityToColor(LogSeverity severity)
    {
        switch (severity) {
            default: return Default;
            case LogSeverity::LogSeverityWarn: return Yellow;
            case LogSeverity::LogSeverityError: return Red;
        }
    }

    void test::log_adapter(LogSeverity severity, const char* err, int len)
    {
        LogTestLabel(console(SeverityToColor(severity), err, len));
    }

    bool test::run_init()
    {
        test_func init; init.name = "init";
        current_func = &init;
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
        current_func = nullptr;
    }

    void test::run_cleanup()
    {
        test_func cleanup; cleanup.name = "cleanup";
        current_func = &cleanup;
        try
        {
            cleanup_test();
        }
        catch (const std::exception& e)
        {
            assert_failed_custom("FAILED with EXCEPTION in [%s]::TestCleanup(): %s\n", name.str, e.what());
        }
        current_func = nullptr;
    }

    bool test::run_test(test_results& results, strview methodFilter)
    {
        current_results = &results; // TODO: thread safety?
        TestVerbosity verb = state().verbosity;

        char title[256];
        int titleLength = 0;

        if (verb >= TestVerbosity::TestLabels)
        {
            titleLength = methodFilter
                ? snprintf(title, sizeof(title), "--------  running '%s.%.*s'  --------", name.str, methodFilter.len, methodFilter.str)
                : snprintf(title, sizeof(title), "--------  running '%s'  --------", name.str);
            consolef(Yellow, "%s\n", title);
        }

        run_init();

        int numTestsRun = 0;
        int numTestsOk = 0;
        int numTestsFailed = 0;

        if (methodFilter)
        {
            for (int i = 0; i < test_count; ++i)
            {
                if (test_funcs[i].name.find(methodFilter))
                {
                    if (run_test_func(results, test_funcs[i])) ++numTestsOk;
                    else                                       ++numTestsFailed;
                    ++numTestsRun;
                }
            }
            if (numTestsRun == 0)
            {
                consolef(Yellow, "No tests matching '%.*s' in %s\n", methodFilter.len, methodFilter.str, name.str);
            }
        }
        else
        {
            for (int i = 0; i < test_count; ++i)
            {
                if (test_funcs[i].autorun)
                {
                    if (run_test_func(results, test_funcs[i])) ++numTestsOk;
                    else                                       ++numTestsFailed;
                    ++numTestsRun;
                }
            }
            if (numTestsRun == 0)
            {
                consolef(Yellow, "No autorun tests discovered in %s\n", name.str);
            }
        }

        run_cleanup();

        bool allSuccess = (numTestsOk == numTestsRun);
        if (verb >= TestVerbosity::TestLabels)
        {
            consolef(Yellow, "%s\n\n", (char*)memset(title, '-', (size_t)titleLength)); // "-------------"
        }
        else if (verb >= TestVerbosity::Summary)
        {
            if (allSuccess)
            {
                std::string run = std::to_string(numTestsOk) + "/" + std::to_string(numTestsRun);
                consolef(Green, "TEST %-32s  %-5s  [OK]\n", name.str, run.c_str());
            }
            else
            {
                std::string run = std::to_string(numTestsFailed) + "/" + std::to_string(numTestsRun);
                consolef(Red,   "TEST %-32s  %-5s  [FAILED]\n", name.str, run.c_str());
            }
        }

        current_results = nullptr; // TODO: thread safety?

        return allSuccess && numTestsRun > 0;
    }

    bool test::run_test_func(test_results& results, test_func& test)
    {
        TestVerbosity verb = state().verbosity;
        if (verb >= TestVerbosity::TestLabels)
        {
            consolef(Yellow, "%s::%s\n", name.str, test.name.str);
        }

        current_func = &test; // TODO: thread safety?
        int before = results.asserts_failed;
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
                if (verb >= TestVerbosity::AllMessages)
                {
                    consolef(Yellow, "Caught Expected Exception in %s::%s\n",
                                     name.str, test.name.str);
                }
            }
            else
            {
                assert_failed_custom("FAILED with EXCEPTION in %s::%s:\n  %s\n",
                                     name.str, test.name.str, e.what());
            }
        }
        current_func = nullptr; // TODO: thread safety?
        int totalFailures = results.asserts_failed - before;
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
        std::vector<std::string> names;
        while (strview pattern = testNamePatterns.next(" \t"))
            names.emplace_back(pattern);
        return run_tests(names);
    }

    int test::run_tests(const std::vector<std::string>& testNamePatterns)
    {
        std::vector<const char*> names;
        names.push_back("");
        for (const std::string& name : testNamePatterns) names.push_back(name.c_str());
        return run_tests((int)names.size(), (char**)names.data());
    }

    int test::run_tests(const char** testNamePatterns, int numPatterns)
    {
        std::vector<const char*> names;
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

    void test::set_verbosity(TestVerbosity verbosity)
    {
        state().verbosity = verbosity;
    }

    int test::add_test_func(test_func func) // @note Because we can't dllexport std::vector
    {
        if (test_count == test_cap)
        {
            test_cap = test_funcs ? test_count * 2 : 8;
            auto* funcs = new test_func[test_cap];
            if (test_funcs) { for (int i = 0; i < test_count; ++i) { funcs[i] = test_funcs[i]; } }
            delete[] test_funcs;
            test_funcs = funcs;
        }
        test_funcs[test_count++] = func;
        return test_count - 1;
    }

    static void set_test_defaults()
    {
        for (test_info& t : state().global_tests)
        {
            if (!t.auto_run)
                t.test_enabled = false;
        }
    }

    static void enable_disable_tests(std::unordered_set<strview>& enabled,
                                     std::unordered_set<strview>& disabled)
    {
        TestVerbosity verb = state().verbosity;

        if (enabled.empty() && disabled.empty())
        {
            consolef(Red, "  No matching tests found for provided arguments!\n");
            for (test_info& t : state().global_tests) // disable all tests to trigger test report warnings
                t.test_enabled = false;
        }
        else if (!disabled.empty())
        {
            for (test_info& t : state().global_tests)
            {
                if (t.auto_run) { // only consider disabling auto_run tests
                    t.test_enabled = disabled.find(t.name) == disabled.end();
                    if (!t.test_enabled && verb >= TestVerbosity::TestLabels)
                    {
                        consolef(Red, "  Disabled %s\n", t.name.to_cstr());
                    }
                }
            }
        }
        else if (!enabled.empty())
        {
            for (test_info& t : state().global_tests) // enable whatever was requested
            {
                t.test_enabled = enabled.find(t.name) != enabled.end();
                if (t.test_enabled && verb >= TestVerbosity::TestLabels)
                {
                    consolef(Green, "  Enabled %s\n", t.name.to_cstr());
                }
            }
        }
    }

    // parses for any flags and removes the from the args vector
    static void parse_flags(std::vector<strview>& args)
    {
        for (auto it = args.begin(); it != args.end(); )
        {
            if (it->starts_with("--verbosity"))
            {
                state().verbosity = (TestVerbosity)it->skip_after('=').to_int();
                it = args.erase(it);
            }
            else ++it;
        }
    }

    /**
     * Valid test arguments:
     * "test_mytest"  enables test with exact match
     * "-test_mytest" disables test with exact match
     * "mytest"     enables test with loose match
     * "-mytest"    disables test with loose match
     * "mytest.mycasefilter"  enables specific test.case
     * "-mytest.mycasefilter" disable specific test.case
     * "*" match all tests  
     * "mytest.*" match mytest and all cases
     * "math -math.numeric" enable all math tests except numeric 
     */
    static void select_tests_from_args(const std::vector<strview>& args)
    {
        // if arg is provided, we assume they are:
        // test_testname or testname or -test_testname or -testname
        // OR to run a specific test:  testname.specifictest
        std::unordered_set<strview> enabled, disabled;
        for (strview arg : args)
        {
            strview testName = arg.next('.');
            strview specific = arg.next('.');

            bool disableTest = false;
            if (testName[0] == '-')
            {
                disableTest = true;
                testName.chomp_first();
            }

            const bool exactMatch = testName.starts_with("test_");
            LogTestLabel(
                if (exactMatch) consolef(Yellow, "Filtering exact  tests '%.*s'\n", arg.len, arg.str);
                else            consolef(Yellow, "Filtering substr tests '%.*s'\n", arg.len, arg.str);
            );

            bool match = false;
            for (test_info& t : state().global_tests)
            {
                if ((exactMatch && t.name == testName) ||
                    (!exactMatch && t.name.find(testName)))
                {
                    t.case_filter = specific;
                    if (disableTest) disabled.insert(t.name);
                    else              enabled.insert(t.name);
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
        TestVerbosity verb = state().verbosity;
        if (verb >= TestVerbosity::TestLabels)
        {
            consolef(Green, "Running all AutoRun tests\n");
        }

        for (test_info& t : state().global_tests)
        {
            if (!t.auto_run && !t.test_enabled && verb >= TestVerbosity::AllMessages)
            {
                consolef(Yellow, "  Disabled NoAutoRun %s\n", t.name.to_cstr());
            }
        }
    }

    test_results run_all_marked_tests()
    {
        test_results results;
        for (test_info& t : state().global_tests)
        {
            if (t.test_enabled)
            {
                auto test = t.factory(t.name);
                if (!test->run_test(results, t.case_filter))
                {
                    results.tests_failed++;
                }
                results.tests_run++;
            }
        }
        return results;
    }

    static int print_final_summary(const test_results& results)
    {
        int numTests = results.tests_run;
        int failed = results.asserts_failed;
        if (failed > 0)
        {
            if (numTests == 1)
                consolef(Red, "\nWARNING: Test failed with %d assertions!\n", failed);
            else
                consolef(Red, "\nWARNING: %d/%d tests failed with %d assertions!\n",
                               results.tests_failed, numTests, failed);
            for (const test_failure& f : results.failures)
            {
                if (f.line) consolef(Red, "    %s:%d  %s::%s:  %s\n", f.file.data(), f.line, f.testname.data(), f.testcase.data(), f.message.data());
                else        consolef(Red, "    %s::%s: %s\n",    f.testname.data(), f.testcase.data(), f.message.data());
            }
            consolef(Default, "\n");
            return -1;
        }
        if (numTests <= 0)
        {
            consolef(Yellow, "\nNOTE: No tests were run! (out of %d available)\n\n", (int)state().global_tests.size());
            return -1;
        }
        if (numTests == 1) consolef(Green, "\nSUCCESS: Test passed!\n\n");
        else               consolef(Green, "\nSUCCESS: All %d tests passed!\n\n", numTests);
        return 0;
    }

    int test::run_tests(int argc, char* argv[])
    {
    #if _WIN32 && _MSC_VER
        #if _CRTDBG_MAP_ALLOC  
            _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF);
            _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
        #endif
    #endif
    
        // filter out invalid empty arguments
        std::vector<strview> args;
        for (int iarg = 1; iarg < argc; ++iarg)
        {
            strview arg = strview{argv[iarg]}.trim();
            if (arg) args.emplace_back(arg);
        }

        set_test_defaults();
        parse_flags(args);

        if (!args.empty())
            select_tests_from_args(args);
        else
            enable_all_autorun_tests();

        test_results testResults = run_all_marked_tests();
        int returnCode = print_final_summary(testResults);
        #if _WIN32 && _MSC_VER
            #if _CRTDBG_MAP_ALLOC
                _CrtDumpMemoryLeaks();
            #endif
        #endif
        return returnCode;
    }

} // namespace rpp

