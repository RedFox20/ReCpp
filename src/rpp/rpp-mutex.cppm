// C++20 module interface unit for <rpp/mutex.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "mutex.h"

export module rpp.mutex;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::mutex;
    using rpp::recursive_mutex;
    using rpp::unlock_guard;
    using rpp::spin_lock;
    using rpp::spin_lock_for;
    using rpp::SyncableType;
    using rpp::synchronize_guard;
    using rpp::synchronizable;
    using rpp::synchronized;
#if RPP_HAS_CRITICAL_SECTION_MUTEX
    using rpp::critical_section;
    using rpp::synchronized_critical;
#endif
}
// GENERATED EXPORTS END
