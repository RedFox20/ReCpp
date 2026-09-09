// C++20 module interface unit for <rpp/close_sync.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "close_sync.h"

export module rpp.close_sync;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::readonly_lock;
    using rpp::exclusive_lock;
    using rpp::close_sync;
}
// GENERATED EXPORTS END
