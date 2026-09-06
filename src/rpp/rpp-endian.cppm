// C++20 module interface unit for <rpp/endian.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "endian.h"

export module rpp.endian;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;

export namespace rpp {
    using rpp::writeBEU16;
    using rpp::writeBEU32;
    using rpp::writeBEU64;
    using rpp::readBEU16;
    using rpp::readBEU32;
    using rpp::readBEU64;
    using rpp::writeLEU16;
    using rpp::writeLEU32;
    using rpp::writeLEU64;
    using rpp::readLEU16;
    using rpp::readLEU32;
    using rpp::readLEU64;
}
// GENERATED EXPORTS END
