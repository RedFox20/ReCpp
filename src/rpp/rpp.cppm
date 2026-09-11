// C++20 umbrella module: `import rpp;` reaches every ReCpp module.
// A macro never crosses a module, so the test macros still need <rpp/tests.macros.h>.
// tools/gen_module_exports.py checks this list against RPP_MODULES_SRC.
export module rpp;

// L0
export import rpp.config;
export import rpp.minmax;
export import rpp.obfuscated_string;
export import rpp.scopeguard;
export import rpp.strview;
export import rpp.debugging;
// L1
export import rpp.bitutils;
export import rpp.traits;
export import rpp.type_traits;
export import rpp.source_loc;
export import rpp.endian;
export import rpp.predicates;
export import rpp.sort;
export import rpp.proc_utils;
export import rpp.future_types;
export import rpp.math;
export import rpp.timepoint;
export import rpp.delegate;
// L2
export import rpp.atomic_timepoint;
export import rpp.collections;
export import rpp.stack_trace;
export import rpp.threads;
export import rpp.timer;
export import rpp.vec;
export import rpp.sprint;
export import rpp.task;
// L3
export import rpp.load_balancer;
export import rpp.memory_pool;
export import rpp.mutex;
export import rpp.paths;
export import rpp.tests;
// L4
export import rpp.atomic_shared_ptr;
export import rpp.close_sync;
export import rpp.condition_variable;
export import rpp.file_io;
export import rpp.sockets;
// L5
export import rpp.binary_stream;
export import rpp.concurrent_queue;
export import rpp.semaphore;
// L6
export import rpp.binary_serializer;
export import rpp.thread_pool;
// L7
export import rpp.future;
export import rpp.event_loop;
// L8
export import rpp.coroutines;
