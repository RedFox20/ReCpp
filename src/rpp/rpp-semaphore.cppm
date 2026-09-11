// C++20 module interface unit for <rpp/semaphore.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "semaphore.h"

export module rpp.semaphore;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::parallel_task_detached;
    using rpp::semaphore;
    using rpp::semaphore_flag;
    using rpp::semaphore_once_flag;
    using rpp::atomic_test_and_set;
}
// GENERATED EXPORTS END
