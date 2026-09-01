// C++20 module interface unit for <rpp/obfuscated_string.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "obfuscated_string.h"

export module rpp.obfuscated_string;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::int32_sequence;
    using rpp::int32_indices;
    using rpp::obfuscated_string;
    using rpp::macro_obfuscated_string;
    using rpp::make_obfuscated;
}
// GENERATED EXPORTS END

// the generator parses with libclang, which defines __clang__, so it never sees this operator
#if __GNUC__ && !__clang__
export namespace rpp { using rpp::operator ""_obfuscated; }
#endif
