# ReCpp [![CircleCI](https://circleci.com/gh/RedFox20/ReCpp.svg?style=svg)](https://circleci.com/gh/RedFox20/ReCpp) (GCC, Clang, MSVC)
## Reusable Standalone C++ Libraries and Modules

The goal of this project is to provide C++17 and C++20 developers with convenient and minimal cross-platform C++ utility libraries.
All of the modules are heavily performance oriented and provides the least amount of overhead possible.

Currently supported and tested platforms are VC++,Clang on Windows and GCC,Clang on Ubuntu,iOS,Android


-----------------------


## [rpp/strview.h](rpp/strview.h) - A lightweight and powerful string utilities
An extremely fast and powerful set of string-parsing utilities. Includes fast tokenization, blazing fast number parsers,
extremely fast number formatters, and basic string search functions and utilities, all extremely well optimized.

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

The `strview` API global utilities:
| Function               | Description                                           
| ------------------- | --------------------------------------------------------
| `double to_double(const char* str, int len, const char** end)` | C-locale specific, simplified atof that also outputs the end of parsed string
| `int to_int(const char* str, int len, const char** end)` | Fast locale agnostic atoi
| `int to_inthx(const char* str, int len, const char** end)` | Fast locale agnostic atoi for HEX strings
| `char* replace(char* str, int len, char chOld, char chNew)` | Replaces characters of 'chOld' with 'chNew' inside the specified string
| `std::string& replace(std::string& str, char chOld, char chNew)` | Replaces characters of 'chOld' with 'chNew' inside this std::string
| `std::string concat(const strview& a, const strview& b, ...)` | Concats multiple strviews
| `char* to_lower(char* str, int len)` | Converts a string into its lowercase form
| `char* to_upper(char* str, int len)` | Converts a string into its uppercase form
| `std::string& to_lower(std::string& str)` | Converts an std::string into its lowercase form
| `std::string& to_upper(std::string& str)` | Converts an std::string into its uppercase form

Utility structs API:
| Class               | Description                                             
| ------------------- | --------------------------------------------------------
| `rpp::line_parser` | Parses an input string buffer for individual lines. The line is returned trimmed of any \r or \n.
| `rpp::keyval_parser` | Parses an input string buffer for 'Key=Value' pairs. The pairs are returned one by one with 'read_next'.
| `rpp::bracket_parser` | Parses an input string buffer for balanced-parentheses structures.

| `rpp::bracket_parser` | Parses an input string buffer for balanced-parentheses structures.

| Class               | Description                                             
| ------------------- | --------------------------------------------------------
| `rpp::strview` | String token for efficient parsing. Represents a 'weak' reference string with Start pointer and Length. The string can be parsed, manipulated and tokenized.

| Function            | Description                                           
| ------------------- | --------------------------------------------------------
| `int to_int()` | Parses this strview as an integer
| `int to_int_hex()` | Parses this strview as a HEX integer ('0xff' or '0ff' or 'ff')
| `long to_long()`
| `float to_float()` | Parses this strview as a float
| `double to_double()`
| `bool to_bool()` | Relaxed parsing of this strview as a boolean: "true", "yes", "on", "1"
| `void clear()` | Clears the strview
| `int size()` | 
| `bool empty()` | 
| `bool is_whitespace()` | 
| `bool is_nullterm()` | 
| `strview& trim_start(...)` | Trims the start of the string
| `strview& trim_end(...)` | Trims the end of the string
| `strview& trim(...)` | Trims both start and end
| `strview& chomp_first()` | Consumes the first character in the strview if possible
| `strview& chomp_last()` | Consumes the last character in the strview if possible
| `char pop_front()` | Pops and returns the first character in the strview if possible
| `char pop_back()` | Pops and returns the last character in the strview if possible
| `bool contains(...)` | TRUE if the strview contains [char, string]
| `bool contains_any(const cahr* chars, int nchars)` | TRUE if the strview contains any of the chars
| `const char* find(...)` | Pointer to start of substring if found, NULL otherwise
| `const char* rfind(...)` | Pointer to char if found using reverse search, NULL otherwise
| `const char* findany(const char* chars, int n)` | Forward searches for any of the specified chars
| `const char* rfindany(const char* chars, int n)` | Reverse searches for any of the specified chars
| `int count(char ch)` | Count number of occurrances of this character inside the strview bounds
| `int indexof(char ch)` | Index of character, or -1 if not found
| `int rindexof(char ch)` | Reverse iterating indexof
| `int indexofany(const char* chars, int n)` | First index of any character that matches Chars array
| `bool starts_with(...)` | TRUE if this strview starts with the specified string
| `bool starts_withi(...)` | TRUE if this strview starts with IGNORECASE of the specified string
| `bool ends_with(...)` | TRUE if the strview ends with the specified string
| `bool ends_withi(...)` | TRUE if this strview ends with IGNORECASE of the specified string
| `bool equals(...)` | TRUE if this strview equals the specified string
| `bool equalsi(...)` | TRUE if this strview equals IGNORECASE the specified string
| `int compare(...)` | Compares this strview to string data
| `strview split_first(...)` | Splits the string into TWO and returns strview to the first one
| `strview split_second(char delim)` | Splits the string into TWO and returns strview to the second one
| `bool next(strview& out, char delim)` | Gets the next strview; also advances the ptr to next token.
| `bool next(strview& out, const char* delims, int ndelims)` | Gets the next string token; also advances the ptr to next token.
| `strview next(char delim)` | Same as bool next(strview& out, char delim), but returns a token instead
| `strview next(const char* delim, int ndelims)`
| `bool next_notrim(strview& out, char delim)` | Gets the next string token; stops buffer on the identified delimiter.
| `bool next_notrim(strview& out, const char* delims, int ndelims)` | Gets the next string token; stops buffer on the identified delimiter.
| `strview next_notrim(char delim)` | Same as bool next(strview& out, char delim), but returns a token instead
| `strview next_notrim(const char* delim, int ndelims)` | 
| `strview substr(int index, int length)` | Tries to create a substring from specified index with given length.
| `strview substr(int index)` | Tries to create a substring from specified index until the end of string.
| `double next_double()` | Parses next float from current strview
| `float next_float()` | 
| `int next_int()` | Parses next int from current Token
| `strview& skip(int nchars)` | Safely chomps N chars while there is something to chomp
| `strview& skip_until(...)` | Skips start of the string until the specified substring is found
| `strview& skip_after(...)` | Skips start of the string until the specified substring is found
| `strview& to_lower()` | Modifies the target string to lowercase
| `std::string as_lower()` | Creates a copy of this strview that is in lowercase
| `strview& to_upper()` | Modifies the target string to be UPPERCASE
| `std::string as_upper()` | Creates a copy of this strview that is in UPPERCASE
| `strview& replace(char chOld, char chNew)` | Modifies the target string by replacing all chOld occurrences with chNew
| `void decompose(auto delim, auto outFirst, auto... outRest)` | Decomposes strview into a struct
```cpp
    rpp::strview input = "user@email.com;27;3486.37;true"_sv;
    User user;
    input.decompose(';', user.email, user.age, user.coins, user.unlocked);
```

-----------------------
-----------------------

## [rpp/file_io.h](rpp/paths.h) - Useful cross platform path utilities (pre C++17)
| Function            | Description                                           
| ------------------- | --------------------------------------------------------
| `bool file_exists(filename)` | TRUE if the file exists, arg ex: "dir/file.ext"
| `bool folder_exists(folder)` | TRUE if the folder exists, arg ex: "root/dir" or "root/dir/"
| `file_or_folder_exists(fileOrFolder)` | TRUE if either a file or a folder exists at the given path
| `bool file_info(path, *size, *creted, *access, *mod)` | Gets basic information of a file
| `int file_size(filename)` | Short size of a file
| `int64 file_sizel(filename)` | Long size of a file
| `time_t file_created(filename)` | File creation date
| `time_t file_accessed(filename)` | Last file access date
| `time_t file_modified(filename)` | Last file modification date
| `bool delete_file(filename)` | Deletes a single file, ex: "root/dir/file.ext"
| `bool copy_file(srcFile, dstFile)` | Copies srcFile to dstFile, overwriting the previous file!
| `bool copy_file_if_needed(srcFile, dstFile)` | Copies srcFile to dstFile IF dstFile doesn't exist
| `bool copy_file_into_folder(srcFile, dstFolder)` | Copies srcFile into dstFolder, overwriting the previous file!
| `bool create_folder(foldername)` | Creates a folder, recursively creating folders that do not exist
| `bool delete_folder(foldername, delete_mode)` | Resolves a relative path to a full path name using filesystem path resolution
| `std::string merge_dirups(path)` | Merges all ../ inside of a path
| `strview file_name(path)` | Extract the filename (no extension) from a file path
| `strview file_nameext(path)` | Extract the file part (with ext) from a file path
| `strview file_ext(path)` | Extract the extension from a file path
| `std::string file_replace_ext(path, ext)` | Replaces the current file path extension
| `std::string file_name_append(path, add)` | Changes only the file name by appending a string
| `std::string file_name_replace(path, newName)` | Replaces only the file name of the path
| `std::string file_nameext_replace(path, newNameAndExt)` | Replaces the file name and extension
| `strview folder_name(path)` | Extract the foldername from a path name
| `strview folder_path(path)` | Extracts the full folder path from a file path
| `std::string& normalize(std::string& path, char sep)` | Normalizes the path string to use a specific type of slash
| `std::string normalized(strview path, char sep)` | Normalizes the path string to use a specific type of slash
| `std::string path_combine(path1, path2, ...)` | Efficiently combines path strings, removing any repeated / or \
| `rpp::dir_iterator` | Basic and minimal directory iterator.
| `list_dirs(dir, recursive, fullpath)` | Lists all folders inside a directory
| `list_files(dir, suffix, recursive, fullpath)` | Lists all files inside this directory that have the specified extension (default: all files)
| `list_alldir(dir, recursive, fullpath)` | Lists all files and folders inside a dir
| `std::string working_dir()` | The current working directory of the application. An extra slash is always appended.
| `std::string module_dir(void* moduleObject)` | Directory where the current module in which Rpp was linked to is located.
| `bool change_dir(const char* new_wd)` | Calls chdir() to set the working directory of the application to a new value
| `std::string temp_dir()` | The system temporary directory for storing misc files
| `std::string home_dir()` | The system home directory for this user

-----------------------
-----------------------

## [rpp/file_io.h](rpp/file_io.h) - Simple and fast File IO
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

-----------------------
-----------------------

## [rpp/delegate.h](rpp/delegate.h) - Fast function delegates and multicast delegates (events)
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

-----------------------
-----------------------

## [rpp/future.h](rpp/future.h) - Composable Futures with C++20 Coroutines Support
This makes it easy to write asynchronous cross-platform software with minimal dependencies.
The main API consists of `rpp::async_task()` which uses `rpp/thread_pool.h` to launch the task in background.
```cpp
template<typename Task>
auto async_task(Task&& task) noexcept -> cfuture<decltype(task())>;
```

The future object `rpp::cfuture<T>` provides composable facilities for continuations.
* `then()` allows chaining futures together by passing the result to the next future and resuming it using rpp::async_task()
* `continue_with()` allows to continue execution without returning a future, which is useful for the final step in chaining
* `detach()` allows to abandon this future by moving the state into rpp::async_task() and waiting for finish there

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
* `cfuture<T> make_read_future(T&& value)` Creates a cfuture<T> which is already completed.
* `cfuture<T> make_exceptional_future(E&& e)` Creates a cfuture<T> which is already errored with the exception.
* `wait_all(const std::vector<cfuture<T>>& vf)` Given a vector of futures, waits blockingly on all of them to be completed.
* `std::vector<T> get_all(const std::vector<cfuture<T>>& vf)` Blocks and gathers the results from all of the futures
* `std::vector<T> get_tasks(std::vector<U>& items, const Launcher& futureLauncher)` Used to launch multiple parallel Tasks and then gather the results.
* `std::vector<T> get_async_tasks(std::vector<U>& items, const Callback& futureCallback)` Used to launch multiple parallel Tasks and then gather the results.
* `void run_tasks(std::vector<U>& items, const Launcher& futureLauncher)` Used to launch multiple parallel Tasks and then wait for the results.

-----------------------
-----------------------

## [rpp/coroutines.h](rpp/coroutines.h) - C++20 Coroutine facilities
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

-----------------------
-----------------------

## [rpp/vec.h](rpp/vec.h) - Everything you need to write a 3D and 2D game in Modern OpenGL
Contains basic `Vector2`, `Vector3`, `Vector4` and `Matrix4` types with a plethora of extremely useful utility functions.
Main features are convenient operators, SSE2 intrinsics and GLSL style vector swizzling.
```cpp
rpp::Vector2 a = { 1.5f, 2.5f };
rpp::Vector2 b = { 2.4f, -5.4f };
rpp::Vector2 dir = (a - b).normalized();
std::cout << dir.toString() << std::endl;
LogInfo("dir: %s\n", dir.toString());
```

-----------------------
-----------------------

## [rpp/timer.h](rpp/timer.h) - Contains cross-platform Timer utilities
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

-----------------------
-----------------------

## [rpp/stack_trace.h](rpp/stack_trace.h) - Cross-platform stack tracing and traced exceptions
Provides detailed stack traces on demand, which is not yet part of C++ standard.
| Function               | Description                                           
| ------------------- | --------------------------------------------------------
| `std::string stack_trace(const char* message, size_t messageLen, int maxDepth, int entriesToSkip)` | Base implementation of stack trace. Only needed if you're implementing custom abstractions
| `std::string stack_trace(int maxDepth)` | Prepares stack trace
| `std::string stack_trace(const char* message, int maxDepth)` | 
| `std::string stack_trace(const std::string& message, int maxDepth)` | Prepares stack trace WITH error message
| `void print_trace(int maxDepth)` | Prints stack trace to STDERR
| `void print_trace(const char* message, int maxDepth)` | 
| `void print_trace(const std::string& message, int maxDepth)` | Prints stack trace to STDERR WITH error message
| `std::runtime_error error_with_trace(const char* message, int maxDepth)` | 
| `std::runtime_error error_with_trace(const std::string& message, int maxDepth)` | Prepared runtime_error with error message and stack trace
| `rpp::traced_exception` | Traced exception forms a complete [message]\\n[stacktrace] string which can be retrieved via runtime_error::what()
| `void register_segfault_tracer()` | Installs a default handler for SIGSEGV which will throw a traced_exception instead of quietly terminating


# CircleCI Local
Running CircleCI locally on WSL/Ubuntu. 
If running on WSL, you need Docker Desktop with WSL2 integration enabled.

You will need to configure a personal API token to use the CircleCI CLI.
Read more at: https://circleci.com/docs/local-cli/#configure-the-cli
```
curl -fLSs https://raw.githubusercontent.com/CircleCI-Public/circleci-cli/master/install.sh | sudo bash
circleci update
circleci setup
```

Executing the build locally, targeting the `android-cpp17` Job:
```
circleci config process .circleci/config.yml > process.yml && \
circleci local execute -c process.yml android-cpp17
```
