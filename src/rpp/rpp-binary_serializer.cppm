// C++20 module interface unit for <rpp/binary_serializer.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "binary_serializer.h"

export module rpp.binary_serializer;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::member_serialize;
    using rpp::serializable;
    using rpp::operator<<;
    using rpp::operator>>;
}
// GENERATED EXPORTS END
