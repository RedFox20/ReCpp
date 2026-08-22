# ReCpp Headers

Every header lives in `src/rpp/`. Test files live in `tests/`.

| Header | Purpose |
|--------|---------|
| `config.h` | Platform detection, compiler macros, base types |
| `strview.h` | Non-owning string view with tokenization and search |
| `sprint.h` | String builder, type-safe formatting, to_string |
| `file_io.h` | Cross-platform file read and write, RAII file handles |
| `paths.h` | Path manipulation, directory listing, filesystem helpers |
| `delegate.h` | Fast function delegates and multicast events |
| `future.h` | Composable futures with continuations and coroutines |
| `future_types.h` | Supporting types for futures |
| `coroutines.h` | C++20 coroutine awaiters and co_await operators |
| `event_loop.h` | Single-threaded coroutine event loop |
| `task.h` | Single-threaded coroutine tasks which resume on the completing thread |
| `thread_pool.h` | Thread pool, parallel_for, parallel_foreach, async tasks |
| `threads.h` | Thread naming, ID queries, CPU core info |
| `mutex.h` | Mutex, spin locks, synchronized<T> wrapper |
| `semaphore.h` | Counting semaphore, semaphore flag, one-shot flag |
| `condition_variable.h` | Condition variable with high-res timeout |
| `concurrent_queue.h` | Thread-safe FIFO queue |
| `close_sync.h` | Read-write sync for safe async destruction |
| `sockets.h` | TCP/UDP sockets, IP addresses, network interfaces |
| `binary_stream.h` | Buffered binary read and write streams |
| `binary_serializer.h` | Reflection-based binary and string serialization |
| `timepoint.h` | Duration, TimePoint, time constants, sleep utilities, duration literals |
| `atomic_shared_ptr.h` | Portable atomic shared_ptr and weak_ptr, with a mutex fallback |
| `atomic_timepoint.h` | Lock-free AtomicDuration, AtomicTimePoint (inherits std::atomic), AtomicTimeSource (time sync and fastforward) |
| `timer.h` | Timer, StopWatch, ScopedPerfTimer (includes timepoint.h) |
| `vec.h` | 2D/3D/4D vector math, matrices, rectangles |
| `math.h` | Clamp, lerp, deg/rad, epsilon compare |
| `minmax.h` | SSE-optimized min, max, abs, sqrt |
| `collections.h` | Container utilities, ranges, algorithms, erasure helpers |
| `debugging.h` | Logging, assertions, log handlers |
| `debugging.macros.h` | The logging and assertion macros, which a C++20 module cannot export |
| `source_loc.h` | Portable call site capture for assert and log messages |
| `stack_trace.h` | Stack tracing and traced exceptions |
| `bitutils.h` | Fixed-length bit array |
| `endian.h` | Endian byte-swap read and write |
| `memory_pool.h` | Linear bump-allocator memory pools |
| `sort.h` | Minimal insertion sort |
| `scope_guard.h` | RAII scope cleanup guard |
| `load_balancer.h` | UDP send rate limiter |
| `obfuscated_string.h` | Compile-time string obfuscation |
| `proc_utils.h` | Process memory and CPU usage info |
| `tests.h` | Minimal unit testing framework |
| `log_colors.h` | ANSI terminal color macros |
| `predicates.h` | C++20 predicate and invocable concepts |
| `type_traits.h` | Detection idiom and type trait helpers |
| `traits.h` | Function type traits for callables |
| `jni_cpp.h` | Android JNI C++ utilities |
