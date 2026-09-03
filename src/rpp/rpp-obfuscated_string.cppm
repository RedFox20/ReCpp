// C++20 module interface unit for <rpp/obfuscated_string.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "obfuscated_string.h"

export module rpp.obfuscated_string;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::obfuscated_string;
    using rpp::make_obfuscated;
}

export namespace rpp::inline literals {
    using rpp::literals::operator""_obfuscated;
}
// GENERATED EXPORTS END
