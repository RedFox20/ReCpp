# ReCpp [![CircleCI](https://circleci.com/gh/RedFox20/ReCpp.svg?style=svg)](https://circleci.com/gh/RedFox20/ReCpp)
## Reusable Standalone C++ Libraries and Modules

The goal of this project is to provide C++17 and C++20 developers with convenient and minimal cross-platform C++ utility libraries.
All of the modules are heavily performance oriented and provides the least amount of overhead possible.

Currently supported and tested platforms are VC++,Clang on Windows and GCC,Clang on Ubuntu,iOS,Android

### [rpp/strview.h](rpp/strview.h) - A lightweight and powerful string utilities
An extremely fast and powerful set of string-parsing utilities. Includes fast tokenization, blazing fast number parsers,
extremely fast number formatters, and not to mention all the basic string search functions and utilities like `starts_with()/contains()/trim()/to_lower()/to_upper()/concat()/replace()/split()/next()`, all optimized to their absolute limits.

This `strview` module works on UTF-8 strings, and since `std::codecvt_utf8` already exists, there is no plan to add a Wide strview.

The string view header also provides examples of basic parsers such as `line_parser`, `keyval_parser` and `bracket_parser`.
It's extremely easy to create blazing fast parsers with `rpp::strview`

```cpp
rpp::strview text = buffer; // or { buffer.data(), buffer.size() };
rpp::strview line;
while (text.next(line, '\n')) // or "\r\n"
{
    line.trim(); // in-place trim
    if (line.empty() || line.starts_with('#'))
        continue;
    if (line.starts_withi("numObJecTS")) // case insensitive search
    {
        objects.reserve(line.to_int()); // to_int(),to_bool(),to_float(),to_double()
    }
    else if (line.starts_with("object")) // "object scene_root 192 768"
    {
        // next(' ') tokenizes the line by skipping over the entries
        line.skip(' '); // skip over "object "
        rpp::strview name = line.next(' '); // "scene_root"
        int posX = line.next_int(); // 192
        int posY = line.next_int(); // 768
        objects.push_back(scene::object{name.to_string()/*std::string*/, posX, posY});
    }
}
```

### [rpp/file_io.h](rpp/file_io.h) - Simple and fast File IO
This is an extremely useful cross-platform file and filesystem module. It provides all the basic
functionality for most common file operations and path manipulations.

### Example of using file_io for basic file manipulation
```cpp
#include <rpp/file_io.h>
using namespace rpp;

void fileio_read_sample(strview filename = "README.md"_sv)
{
    if (file f = { filename, READONLY })
    {
        // reads all data in the most efficient way
        load_buffer data = f.read_all(); 
        
        // use the data as a binary blob
        for (char& ch : data) { }
    }
}
void fileio_writeall_sample(strview filename = "test.txt"_sv)
{
    string someText = "/**\n * A simple self-expanding buffer\n */\nstruct";
    
    // write a new file with the contents of someText
    file::write_new(filename, someText.data(), someText.size());
    
    // or just write it as a string 
    file::write_new(filename, someText);
}
void fileio_info_sample(strview file = "README.md"_sv)
{
    if (file_exists(file))
    {
        printf(" === %s === \n", file.str);
        printf("  file_size     : %d\n",   file_size(file));
        printf("  file_modified : %llu\n", file_modified(file));
        printf("  full_path     : %s\n",   full_path(file).data());
        printf("  file_name     : %s\n",   file_name(file).data());
        printf("  folder_name   : %s\n",   folder_name(full_path(file)).data());
    }
}
void fileio_path_manipulation(strview file = "/root/../path\\example.txt"_sv)
{
    printf(" === %s === \n", file.str);
    printf("  full_path     : %s\n", full_path(file).data());
    printf("  merge_dirups  : %s\n", merge_dirups(file).data());
    printf("  file_name     : %s\n", file_name(file).str);
    printf("  file_nameext  : %s\n", file_nameext(file).str);
    printf("  file_ext      : %s\n", file_ext(file).str);
    printf("  folder_name   : %s\n", file_ext(file).str);
    printf("  folder_path   : %s\n", file_ext(file).str);
    printf("  normalized    : %s\n", normalized(file).data());
    printf("  path_combine  : %s\n", path_combine(folder_name(file), "another.txt").data());
}
void fileio_listing_dirs(strview path = "../"_sv)
{
    printf(" working dir   : %s\n", working_dir().data());
    printf(" relative path : %s\n", path.str);
    printf(" full path     : %s\n", full_path(path).data());
    
    vector<string> cppFiles = list_files_recursive(path, "cpp");
    for (auto& relativePath : cppFiles)
        printf("  source  : %s\n", relativePath.data());
    
    vector<string> headerFiles = list_files_fullpath("./rpp", "h");
    for (auto& fullPath : headerFiles)
        printf("  header  : %s\n", fullPath.data());
}
```

### [rpp/delegate.h](rpp/delegate.h) - Fast function delegates and multicast delegates (events)
| Class               | Description                                              |
| ------------------- | -------------------------------------------------------- |
| `delegate<f(a)>`  | Function delegate that can contain static functions, instance member functions, lambdas and functors. |
| `event<void(a)>`  | Multicast delegate object, acts as an optimized container for registering multiple delegates to a single event. |
### Examples of using delegates for any convenient case
```cpp
void delegate_samples()
{
    // create a new function delegate - in this case using a lambda
    delegate<void(int)> fn = [](int a) { 
        printf("lambda %d!\n", a); 
    };
    
    // invoke the delegate like any other function
    fn(42);
}
void event_sample()
{
    // create an event container
    event<void(int,int)> onMouseMove;

    // add a single notification target (more can be added if needed)
    onMouseMove += [](int x, int y) { 
        printf("mx %d, my %d\n", x, y); 
    };
    
    // call the event and multicast to all registered callbacks
    onMouseMove(22, 34);
}
```

### [rpp/future.h](rpp/future.h) - Composable Futures with C++20 Coroutines Support
This makes it easy to write asynchronous cross-platform software with minimal dependencies.
The main API consists of `rpp::async_task()` which uses `rpp/thread_pool.h` to launch the task in background.
```cpp
template<typename Task>
auto async_task(Task&& task) noexcept -> cfuture<decltype(task())>;
```

The future object `rpp::cfuture<T>` provides composable facilities for continuations.
* `then()` - allows chaining futures together by passing the result to the next future and resuming it using rpp::async_task()
* `continue_with()` - allows to continue execution without returning a future, which is useful for the final step in chaining
* `detach()` - allows to abandon this future by moving the state into rpp::async_task() and waiting for finish there

Example with classic composable futures using callbacks
```cpp
    rpp::async_task([=]{
        return downloadZipFile(url);
    }).then([=](std::string zipPath) {
        return extractContents(zipPath);
    }).continue_with([=](std::string extractedDir) {
        callOnUIThread([=]{ jobComplete(extractedDir); });
    });
```

Continuations with exception handlers: allows to forward the task result to the next task in chain, or handle an exception of the given type. This allows for more complicated task chains where exceptions can be used to recover the task chain and return a usable value.
```cpp
    rpp::async_task([=] {
        return loadCachedScene(getCachePath(file));
    }, [=](invalid_cache_state& e) {
        return loadCachedScene(downloadAndCacheFile(file)); // recover
    }).then([=](std::shared_ptr<SceneData> sceneData) {
        setCurrentScene(sceneData);
    }, [=](scene_load_failed& e) {
        loadDefaultScene(); // recover
    });
```

Example with C++20 coroutines. This uses co_await <lambda> to run each lambda
in a background thread. This is useful if you are upgrading legacy Synchronous
code which doesn't have awaitable IO.

The lambdas are run in background thread using `rpp::thread_pool`
```cpp
using namespace rpp::coro_operators;
rpp::cfuture<void> downloadAndUnzipDataAsync(std::string url)
{
    std::string zipPath = co_await [&]{ return downloadZipFile(url); };
    std::string extractedDir = co_await [&]{ return extractContents(zipPath); };
    co_await callOnUIThread([=]{ jobComplete(extractedDir); });
    co_return;
}
rpp::cfuture<void> coroRunnerExample()
{
    co_await downloadAndUnzipDataAsync("https://example.com");
}
```
Some additional utilities
* `cfuture<T> make_read_future(T&& value)` - Creates a cfuture<T> which is already completed.
* `cfuture<T> make_exceptional_future(E&& e)` - Creates a cfuture<T> which is already errored with the exception.
* `wait_all(const std::vector<cfuture<T>>& vf)` - Given a vector of futures, waits blockingly on all of them to be completed.
* `std::vector<T> get_all(const std::vector<cfuture<T>>& vf)` - Blocks and gathers the results from all of the futures
* `std::vector<T> get_tasks(std::vector<U>& items, const Launcher& futureLauncher)` - Used to launch multiple parallel Tasks and then gather the results.
* `std::vector<T> get_async_tasks(std::vector<U>& items, const Callback& futureCallback)` - Used to launch multiple parallel Tasks and then gather the results.
* `void run_tasks(std::vector<U>& items, const Launcher& futureLauncher)` - Used to launch multiple parallel Tasks and then wait for the results.

### [rpp/coroutines.h](rpp/coroutines.h) - C++20 Coroutine facilities
Contains basic utilities for using C++20 coroutines. Supports MSVC++, GCC and Clang.
* `rpp::lambda_awaiter<Task>` - Spawns a parallel task to enable awaiting on any lambda.
```cpp
rpp::cfuture<void> lambda_awaiter_example() {
    using namespace rpp::coro_operators;
    std::string zip = co_await [&]{ return downloadFile(url); };
    co_await [&]{ unzipArchive(zip, "."); }
}
```
* `rpp::chrono_awaiter<Clock>` - Spawns a parallel task to sleep and notify until enough time has elapsed.
```cpp
    using namespace rpp::coro_operators;
    co_await std::chrono::milliseconds{100};
```

### [rpp/vec.h](rpp/vec.h) - Everything you need to write a 3D and 2D game in Modern OpenGL
Contains basic `Vector2`, `Vector3`, `Vector4` and `Matrix4` types with a plethora of extremely useful utility functions.
Main features are convenient operators, SSE2 intrinsics and GLSL style vector swizzling.
```cpp
rpp::Vector2 a = { 1.5f, 2.5f };
rpp::Vector2 b = { 2.4f, -5.4f };
rpp::Vector2 dir = (a - b).normalized();
std::cout << dir.toString() << std::endl;
LogInfo("dir: %s\n", dir.toString());
```

### [rpp/timer.h](rpp/timer.h) - Contains cross-platform Timer utilities
Basic timer utilities for measuring time for performance profiling and consistent thread sleeps.
```cpp
    rpp::Timer t;
    doWork();
    double elapsed = t.elapsed();
    printf("work elapsed: %.2fs\n", elapsed);
```

* `rpp::Timer` High accuracy timer for performance profiling or deltaTime measurement.
  * `Timer()` Initializes a new timer by calling start
  * `Timer(StartMode startMode)` Explicitly defines `Timer::NoStart` or `Timer::AutoStart`
  * `void start()` Starts the timer
  * `double elapsed()` Fractional seconds elapsed from start()
  * `double elapsed_ms()` Fractional milliseconds elapsed from start()
  * `double next()` Gets the next time sample, since the last call to next() or start() and calls start() again
  * `double next_ms()` next() converted to milliseconds
  * `double measure(Func f)` Measure block execution time as seconds
  * `double measure_ms(Func f)` Measure block execution time as milliseconds

* `rpp::StopWatch` High accuracy stopwatch for measuring specific events and keeping the results
  * `StopWatch` Creates an uninitialized StopWatch. Reported time is always 0.0
  * `void start()` Sets the initial starting point of the stopwatch and resets the stop point only if the stopwatch hasn't started already
  * `void stop()` Sets the stop point of the stopwatch only if start point exists and not already stopped
  * `void resume()` Clears the stop point and resumes timing
  * `void reset()` Resets both start and stop times
  * `bool started()` Has the stopwatch been started?
  * `bool stopped()` Has the stopwatch been stopped with a valid time?
  * `double elapsed()` Reports the currently elapsed time.
  * `double elapsed_ms()` Currently elpased time in milliseconds

* `rpp::ScopedPerfTimer` Automatically logs performance from constructor to destructor and writes it to log
  * `ScopedPerfTimer(const char* what)` Starts the timer
  * `~ScopedPerfTimer()` Prints out the elapsed time

Global time utilities:
* `uint64_t time_now()` Gets the current timestamp from the system's most accurate time measurement
* `double time_period()` Gets the time period for the system's most accurate time measurement
* `double time_now_seconds()` Current time in seconds:  rpp::time_now() * rpp::time_period()
* `int64_t from_sec_to_time_ticks(double seconds)` Converts fractional seconds to clock ticks that matches time_now()
* `int64_t from_ms_to_time_ticks(double millis)` Converts fractional milliseconds int to clock ticks that matches time_now()
* `int64_t from_us_to_time_ticks(double micros)` Converts fractional microseconds int to clock ticks that matches time_now()
* `double time_ticks_to_sec(int64_t ticks)` Converts clock ticks that matches time_now() into fractional seconds
* `double time_ticks_to_ms(int64_t ticks)` Converts clock ticks that matches time_now() into fractional milliseconds
* `double time_ticks_to_us(int64_t ticks)` Converts clock ticks that matches time_now() into fractional microseconds
* `void sleep_ms(unsigned int millis)` Let this thread sleep for provided milliseconds
* `void sleep_us(unsigned int microseconds)` Let this thread sleep for provided MICROSECONDS


### [rpp/stack_trace.h](rpp/stack_trace.h) - Cross-platform stack tracing and traced exceptions

* `std::string stack_trace(const char* message, size_t messageLen, int maxDepth, int entriesToSkip)`
    - Base implementation of stack trace. Only needed if you're implementing custom abstractions
* `std::string stack_trace(int maxDepth = 32)`
    - Prepares stack trace
* `std::string stack_trace(const char* message, int maxDepth = 32)`
* `std::string stack_trace(const std::string& message, int maxDepth = 32)`
    - Prepares stack trace WITH error message
* `void print_trace(int maxDepth = 32)`
    - Prints stack trace to STDERR
* `void print_trace(const char* message, int maxDepth = 32)`
* `void print_trace(const std::string& message, int maxDepth = 32)`
    - Prints stack trace to STDERR WITH error message
* `std::runtime_error error_with_trace(const char* message, int maxDepth = 32)`
* `std::runtime_error error_with_trace(const std::string& message, int maxDepth = 32)`
    - Prepared runtime_error with error message and stack trace
* `rpp::traced_exception`
    - Traced exception forms a complete [message]\\n[stacktrace] string which can be retrieved via runtime_error::what()
* `void register_segfault_tracer()`
    - Installs a default handler for SIGSEGV which will throw a traced_exception instead of quietly terminating
