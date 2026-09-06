// C++20 module interface unit for <rpp/delegate.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "delegate.h"

export module rpp.delegate;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;

export namespace rpp {
    using rpp::delegate;
    using rpp::multicast_delegate;
    using rpp::multicast_fwd;
    using rpp::multicast_fwd_t;
}
// GENERATED EXPORTS END
