#include "tests.h"
#include <memory>
#include <rpp/mutex.h>
#include <unordered_set>
#include <chrono> // high_resolution_clock
#include <cstdarg>
#include <cassert>
#include <thread>

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
        bool repeat = false;
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
        void unmap()
        {
            if (!shared_mem)
                return;
            if (shared_mem->initialized)
            {
                shared_mem->initialized = false;
                shared_mem->stored_object.~T();
            }
            #if _MSC_VER
                UnmapViewOfFile(shared_mem);
                CloseHandle(handle);
                handle = INVALID_HANDLE_VALUE;
            #elif __ANDROID__
                delete shared_mem;
            #else // _WIN32
                munmap(shared_mem, sizeof(shared));
                close(shm_fd);
                shm_fd = 0;
            #endif
            shared_mem = nullptr;
        }
    };

    static shared_memory_view<mapped_state>& get_mapped_state()
    {
        static shared_memory_view<mapped_state> s;
        return s;
    }

    static mapped_state& state()
    {
        return get_mapped_state().get();
    }

    void register_test(strview name, test_factory factory, bool autorun)
    {
        state().global_tests.emplace_back(test_info{ name, factory, {}, true, autorun });
    }

    ///////////////////////////////////////////////////////////////////////////

    struct test_failure
    {
        rpp::strview testname; // major test name
        rpp::strview testcase; // failed case name
        std::string message;
        rpp::strview file;
        int line;
    };

    struct test_results
    {
        int tests_run = 0;
        int tests_failed = 0;
        int asserts_failed = 0;
        std::vector<test_failure*> failures;
        std::chrono::nanoseconds elapsed_time{};
        std::shared_ptr<rpp::mutex> mutex = std::make_shared<rpp::mutex>();

        ~test_results()
        {
            for (test_failure* f : failures) delete f;
        }
    };

    struct message
    {
        uint32_t start_index;
        uint16_t len;
        uint8_t type;
    };

    struct test::test_func
    {
        strview name;
        rpp::mutex mutex{};
        test_func_type func { nullptr };
        size_t expectedExType = 0;
        std::chrono::nanoseconds elapsed_time{};
        bool autorun = true;
        bool did_run = false;
        bool success = false;
    private:
        rpp::string_buffer message_buf;
        std::vector<message> messages;
    public:
        size_t num_messages() const noexcept { return messages.size(); }
        const message& get_message(size_t i) const noexcept { return messages[i]; }
        rpp::strview get_message_text(const message& m) const
        {
            const char* start = message_buf.data() + m.start_index;
            return { start, start + m.len };
        }
        void append_message(int type, const char* msg, int len)
        {
            std::lock_guard lock {mutex};
            message m;
            m.start_index = message_buf.size(); // we start at current stringbuf pos
            m.len = (uint16_t)len;
            m.type = (uint8_t)type;
            messages.push_back(m);
            message_buf.append(msg, len); // push the string data
        }
    };

    struct test::test_impl
    {
        std::vector<test_func*> test_functions;
        test_results* current_results = nullptr;
        test_func* current_func = nullptr;
        std::chrono::nanoseconds elapsed_time{};

        ~test_impl()
        {
            for (test_func* fn : test_functions) delete fn;
        }
    };


    ///////////////////////////////////////////////////////////////////////////

    // current test func is thread-local, so that tests could be run in parallel
    static thread_local test::test_func* tl_current_test_func;

    test::test(strview name) : name{ name }
    {
        impl = new test_impl();
    }
    test::~test() noexcept
    {
        delete impl;
    }

    enum ConsoleColor { Default, Green, Yellow, Red, };

    static void console(ConsoleColor color, const char* str, int len)
    {
    #if _WIN32
        FILE* cout = color == Red ? stderr : stdout;

        static constexpr const char* colors[] = {
            "\x1b[0m",  // clears all formatting
            "\x1b[32m", // green
            "\x1b[33m", // yellow
            "\x1b[31m", // red
        };
        static rpp::mutex consoleSync;
        {
            std::lock_guard lock { consoleSync };
            fwrite(colors[color], strlen(colors[color]), 1, cout);
            fwrite(str, size_t(len), 1, cout);
            fwrite(colors[Default], strlen(colors[Default]), 1, cout);
        }

        fflush(cout); // flush needed for proper sync with unix-like shells
    #elif __ANDROID__
        (void)len;
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
            static constexpr const char* colors[] = {
                "\x1b[0m",  // clears all formatting
                "\x1b[32m", // green
                "\x1b[33m", // yellow
                "\x1b[31m", // red
            };
            static rpp::mutex consoleSync;
            {
                std::unique_lock guard{ consoleSync, std::defer_lock };
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

        if (impl->current_results)
            add_assert_failure(filename, line, msg, len);
    }

    void test::assert_failed_custom(const char* fmt, ...)
    {
        safe_vsnprintf_msg_len(fmt);
        LogTestLabel(console(Red, msg, len));

        if (impl->current_results)
            add_assert_failure("", 0, msg, len);
    }

    void test::add_assert_failure(const char* file, int line, const char* msg, int len)
    {
        test_failure* fail = new test_failure{};
        fail->testname = name;
        fail->testcase = impl->current_func->name;
        fail->message = std::string{msg,msg+len};
        fail->file = rpp::strview{file};
        fail->line = line;
        {
            std::lock_guard lock {*impl->current_results->mutex};
            impl->current_results->asserts_failed++;
            impl->current_results->failures.push_back(fail);
        }
    }

    void test::add_message(int type, const char* msg, int len)
    {
        if (auto* func = tl_current_test_func)
            func->append_message(type, msg, len);
    }

    void test::print_error(const char* fmt, ...)
    {
        safe_vsnprintf_msg_len(fmt);
        if (state().verbosity >= TestVerbosity::TestLabels) { 
            console(Red, msg, len);
        }
        add_message(2, msg, len);
    }

    void test::print_warning(const char* fmt, ...)
    {
        safe_vsnprintf_msg_len(fmt);
        if (state().verbosity >= TestVerbosity::AllMessages) { 
            console(Yellow, msg, len);
        }
        add_message(1, msg, len);
    }

    void test::print_info(const char* fmt, ...)
    {
        safe_vsnprintf_msg_len(fmt);
        if (state().verbosity >= TestVerbosity::AllMessages) { 
            console(Default, msg, len);
        }
        add_message(0, msg, len);
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
        test_func init;
        init.name = "init";
        impl->current_func = &init;
        try
        {
            init_test();
            impl->current_func = nullptr;
            return true;
        }
        catch (const std::exception& e)
        {
            assert_failed_custom("FAILED with EXCEPTION in [%s]::TestInit(): %s\n", name.str, e.what());
            impl->current_func = nullptr;
            return false;
        }
    }

    void test::run_cleanup()
    {
        test_func cleanup;
        cleanup.name = "cleanup";
        impl->current_func = &cleanup;
        try
        {
            cleanup_test();
        }
        catch (const std::exception& e)
        {
            assert_failed_custom("FAILED with EXCEPTION in [%s]::TestCleanup(): %s\n", name.str, e.what());
        }
        impl->current_func = nullptr;
    }

    static const char* get_time_str(const std::chrono::nanoseconds& time) noexcept
    {
        static thread_local char buffer[128];
        if (time.count() < 1000ll) // less than 1us, display as 999ns
        {
            snprintf(buffer, sizeof(buffer), "%dns", (int)time.count());
        }
        else if (time.count() < 1'000'000ll) // less than 1ms, display as 999us
        {
            snprintf(buffer, sizeof(buffer), "%dus", (int)(time.count() / 1'000ll));
        }
        else if (time.count() < 50'000'000ll) // less than 50ms, display as 49.99ms
        {
            snprintf(buffer, sizeof(buffer), "%.2fms", time.count() / 1'000'000.0);
        }
        else if (time.count() < 1'000'000'000ll) // less than 1sec, display as 899.9ms
        {
            snprintf(buffer, sizeof(buffer), "%.1fms", time.count() / 1'000'000.0);
        }
        else if (time.count() < 60'000'000'000ll) // less than 1min, display as 59.91s
        {
            snprintf(buffer, sizeof(buffer), "%.2fs", time.count() / 1'000'000'000.0);
        }
        else // over 1 min, display as 120m 59s
        {
            int minutes = (int)(time.count() / 60'000'000'000ll);
            int seconds = (int)((time.count() % 60'000'000'000ll) / 1'000'000'000ll);
            snprintf(buffer, sizeof(buffer), "%dm %ds", minutes, seconds);
        }
        return buffer;
    }

    bool test::run_test(test_results& results, strview methodFilter)
    {
        impl->current_results = &results;
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

        auto t1 = std::chrono::high_resolution_clock::now();
        run_init();

        int numTestsRun = 0;
        int numTestsOk = 0;
        int numTestsFailed = 0;

        if (methodFilter)
        {
            for (test_func* fn : impl->test_functions)
            {
                if (fn->name.find(methodFilter))
                {
                    if (run_test_func(*fn)) ++numTestsOk;
                    else                    ++numTestsFailed;
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
            for (test_func* fn : impl->test_functions)
            {
                if (fn->autorun)
                {
                    if (run_test_func(*fn)) ++numTestsOk;
                    else                    ++numTestsFailed;
                    ++numTestsRun;
                }
            }
            if (numTestsRun == 0)
            {
                consolef(Yellow, "No autorun tests discovered in %s\n", name.str);
            }
        }

        run_cleanup();
        auto t2 = std::chrono::high_resolution_clock::now();
        impl->elapsed_time = t2 - t1;

        const bool allSuccess = (numTestsOk == numTestsRun);
        if (verb >= TestVerbosity::Summary)
        {
            if (allSuccess)
            {
                char run[64]; snprintf(run, sizeof(run), "%d/%d", numTestsOk, numTestsRun);
                consolef(Green, "TEST %-32s  %-5s  [OK] %s\n", name.str, run, get_time_str(impl->elapsed_time));
            }
            else
            {
                char run[64]; snprintf(run, sizeof(run), "%d/%d", numTestsFailed, numTestsRun);
                consolef(Red,   "TEST %-32s  %-5s  [FAILED] %s\n", name.str, run, get_time_str(impl->elapsed_time));
            }
        }

        if (verb >= TestVerbosity::TestLabels)
        {
            consolef(Yellow, "%s\n\n", (char*)memset(title, '-', (size_t)titleLength)); // "-------------"
        }
        else if (verb >= TestVerbosity::Summary && !allSuccess)
        {
            for (test_func* fn : impl->test_functions)
            {
                if (!fn->did_run || fn->success)
                    continue;
                consolef(Red, "FAIL %s::%s\n", name.str, fn->name.str);
                for (size_t i = 0; i < fn->num_messages(); ++i)
                {
                    const message& m = fn->get_message(i);
                    rpp::strview msg = fn->get_message_text(m);
                    ConsoleColor c = Default;
                    if      (m.type == 1) c = Yellow;
                    else if (m.type == 2) c = Red;
                    console(c, msg.str, msg.len);
                }
            }
        }

        impl->current_results = nullptr;

        return allSuccess && numTestsRun > 0;
    }

    bool test::run_test_func(test_func& test)
    {
        TestVerbosity verb = state().verbosity;
        if (verb >= TestVerbosity::TestLabels)
        {
            consolef(Yellow, "%s::%s\n", name.str, test.name.str);
        }

        test.success = false;
        test.did_run = true;
        tl_current_test_func = impl->current_func = &test;
        int before = impl->current_results->asserts_failed;
        auto t1 = std::chrono::high_resolution_clock::now();
        decltype(t1) t2;
        try
        {
            #if _MSC_VER
                (reinterpret_cast<dummy*>(this)->*test.func.dfunc)();
            #else
                test.func.mfunc(this);
            #endif
            t2 = std::chrono::high_resolution_clock::now();
            if (test.expectedExType) // we expected an exception, but none happened?!
            {
                assert_failed_custom("FAILED with expected EXCEPTION NOT THROWN in %s::%s\n",
                                     name.str, test.name.str);
            }
        }
        catch (const std::exception& e)
        {
            t2 = std::chrono::high_resolution_clock::now();
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

        tl_current_test_func = impl->current_func = nullptr;
        test.elapsed_time = t2 - t1;

        int totalFailures = impl->current_results->asserts_failed - before;
        test.success = totalFailures <= 0;

        if (verb >= TestVerbosity::TestLabels)
        {
            if (test.success)
                consolef(Green, "%s::%s OK %s\n", name.str, test.name.str, get_time_str(test.elapsed_time));
            else
                consolef(Red, "%s::%s FAILED %s\n", name.str, test.name.str, get_time_str(test.elapsed_time));
        }

        return test.success;
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

    void test::spin_sleep_for(double seconds) noexcept
    {
        spin_sleep_for_us(static_cast<uint64_t>(seconds * 1'000'000.0));
    }

    void test::spin_sleep_for_ms(uint64_t milliseconds) noexcept
    {
        spin_sleep_for_us(milliseconds * 1000ull);
    }

    void test::spin_sleep_for_us(uint64_t microseconds) noexcept
    {
        // NOTE: high_resolution_clock might not be available on some target systems
        //       which would require a fallback to rolling our own.
        //       However, we want to avoid using rpp/timer.h here, since that would
        //       introduce a circular dependency in the testing framework and not test
        //       the timer.h implementation.
        namespace time = std::chrono;
        auto start = time::high_resolution_clock::now();
        auto end = start + time::nanoseconds(microseconds * 1000ull);
        while (time::high_resolution_clock::now() < end)
        {
        }
    }

    void test::cleanup_all_tests()
    {
        get_mapped_state().unmap();
    }

    void test::set_verbosity(TestVerbosity verbosity)
    {
        state().verbosity = verbosity;
    }

    int test::add_test_func(strview name, test_func_type fn, 
                            size_t expectedExHash, bool autorun)
    {
        test_func* func = new test_func{};
        func->name = name;
        func->func = fn;
        func->expectedExType = expectedExHash;
        func->autorun = autorun;
        impl->test_functions.push_back(func);
        return static_cast<int>(impl->test_functions.size()) - 1;
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

    static void print_help()
    {
        consolef(Default, "Usage: test_runner [options] [testname] [testname.testcase]\n");
        consolef(Default, "Options:\n");
        consolef(Default, "  -h, --help        Print this help message\n");
        consolef(Default, "  -v, -vv           Increase verbosity level [none:0 default:1 -v:2 -vv:3]\n");
        consolef(Default, "  --verbosity=3     Set verbosity level [default:1(Summary) 2(Labels) 3(All)]\n");
        consolef(Default, "  -r, --repeat      Repeat tests until a failure occurs\n");
        consolef(Default, "  testname           Run test suite (loose match)\n");
        consolef(Default, "  -testname          Exclude test suite (loose match)\n");
        consolef(Default, "  test:testname      Run test suite (exact match)\n");
        consolef(Default, "  -test:testname     Exclude test suite (exact match)\n");
        consolef(Default, "  testname.testcase  Run a specific test case\n");
        consolef(Default, "  -testname.testcase Exclude a specific test case\n");
        consolef(Default, "  *                  Run all tests\n");
        consolef(Default, "  testname.*         Run all test cases in a suite\n");
        consolef(Default, "  testname -testname.testcase  Run all tests except a specific case\n");
        
    }

    // parses for any flags and removes the from the args vector
    static void parse_flags(std::vector<strview>& args)
    {
        for (auto it = args.begin(); it != args.end(); )
        {
            if (*it == "-h" || *it == "--help")
            {
                print_help();
                std::exit(0);
            }
            else if (it->starts_with("--verbosity"))
            {
                if (*it == "--verbosity") // --verbosity 3
                {
                    it = args.erase(it);
                    if (it == args.end()) break;
                    state().verbosity = (TestVerbosity)it->to_int();
                    it = args.erase(it);
                }
                else // --verbosity=3
                {
                    state().verbosity = (TestVerbosity)it->skip_after('=').to_int();
                    it = args.erase(it);
                }
            }
            else if (*it == "-v" || *it == "-vv")
            {
                // Summary is the default, so -v will bump it once, -vv will be super verbose
                state().verbosity = *it == "-v" ? TestVerbosity::TestLabels
                                                : TestVerbosity::AllMessages;
                it = args.erase(it);
            }
            else if (*it == "-r" || *it == "--repeat")
            {
                state().repeat = true;
                it = args.erase(it);
            }
            else
            {
                ++it;
            }
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

            const bool exactMatch = testName.starts_with("test:");
            testName = exactMatch ? testName.skip_after(':') : testName;
            LogTestLabel(
                if (exactMatch) consolef(Yellow, "Filtering exact  tests '%.*s'\n", arg.len, arg.str);
                else            consolef(Yellow, "Filtering substr tests '%.*s'\n", arg.len, arg.str);
            );

            bool match = false;
            for (test_info& t : state().global_tests)
            {
                if ((exactMatch && t.name.equalsi(testName)) ||
                    (!exactMatch && t.name.containsi(testName)))
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

    void run_all_marked_tests(test_results& results)
    {
        auto t1 = std::chrono::high_resolution_clock::now();
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

        auto t2 = std::chrono::high_resolution_clock::now();
        results.elapsed_time = t2 - t1;
    }

    static int print_final_summary(const test_results& results)
    {
        int numTests = results.tests_run;
        int failed = results.asserts_failed;
        if (failed > 0)
        {
            if (numTests == 1)
                consolef(Red, "\nWARNING: Test failed with %d assertions! %s\n", failed, get_time_str(results.elapsed_time));
            else
                consolef(Red, "\nWARNING: %d/%d tests failed with %d assertions! %s\n",
                               results.tests_failed, numTests, failed, get_time_str(results.elapsed_time));
            for (const test_failure* f : results.failures)
            {
                if (f->line) consolef(Red, "    %s:%d  %s::%s:  %s\n", f->file.data(), f->line, f->testname.data(), f->testcase.data(), f->message.data());
                else         consolef(Red, "    %s::%s: %s\n", f->testname.data(), f->testcase.data(), f->message.data());
            }
            consolef(Default, "\n");
            return failed;
        }
        if (numTests <= 0)
        {
            consolef(Yellow, "\nNOTE: No tests were run! (out of %d available)\n\n", (int)state().global_tests.size());
            return 1;
        }
        if (numTests == 1) consolef(Green, "\nSUCCESS: Test passed! %s\n\n", get_time_str(results.elapsed_time));
        else               consolef(Green, "\nSUCCESS: All %d tests passed! %s\n\n", numTests, get_time_str(results.elapsed_time));
        return 0;
    }

    int test::run_tests(strview testNamePatterns)
    {
        char* patterns[128] = { nullptr };
        int npatterns = 0;
        patterns[npatterns++] = (char*)"";

        strview pattern;
        while (testNamePatterns.next_skip_empty(pattern, " \t"))
        {
            if (npatterns >= 128) LogError("Too many test patterns");
            else patterns[npatterns++] = (char*)pattern.data();
        }
        return run_tests(npatterns, patterns);
    }

    int test::run_tests(const char** testNamePatterns, int numPatterns)
    {
        char* patterns[128] = { nullptr };
        int npatterns = 0;
        patterns[npatterns++] = (char*)"";

        for (int i = 0; i < numPatterns; ++i)
        {
            if (npatterns >= 128) LogError("Too many test patterns");
            else patterns[numPatterns++] = (char*)testNamePatterns[i];
        }
        return run_tests(npatterns, patterns);
    }

    int test::run_tests()
    {
        char empty[1] = "";
        char* argv[1] = { empty };
        return run_tests(1, argv);
    }

    int test::run_tests(int argc, char* argv[])
    {
        #if RPP_MSVC_WIN && _CRTDBG_MAP_ALLOC
            _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF);
            _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
        #endif
        #if RPP_GCC // for GCC, set a verbose terminate() handler
            std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
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

        test_results results{};
        int returnCode = -1;
        try
        {
            while (true)
            {
                run_all_marked_tests(results);
                returnCode = print_final_summary(results);
                if (returnCode == 0 && state().repeat)
                {
                    // cleanup all state and go for another loop
                    results = {};
                    continue;
                }
                break;
            }
        }
        catch (const std::exception& e)
        {
            consolef(Red, "Unhandled exception while running tests: %s\n", e.what());
            return 1;
        }

        #if _WIN32 && _MSC_VER
            #if _CRTDBG_MAP_ALLOC
                _CrtDumpMemoryLeaks();
            #endif
        #endif
        return returnCode;
    }

} // namespace rpp

