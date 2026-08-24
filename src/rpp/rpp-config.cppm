// C++20 module interface unit for the rpp integer aliases, wrapping <rpp/config.types.h>.
// config.h holds the macros, which a module cannot export, so the types live in config.types.h.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "config.types.h"

export module rpp.config;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::byte;
    using rpp::ushort;
    using rpp::uint;
    using rpp::ulong;
    using rpp::int16;
    using rpp::uint16;
    using rpp::int32;
    using rpp::uint32;
    using rpp::int64;
    using rpp::uint64;
    using rpp::__wrap;
}
// GENERATED EXPORTS END
