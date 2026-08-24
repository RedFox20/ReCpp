// C++20 module interface unit for rpp::strview, wrapping <rpp/strview.h>.
// A module cannot export a macro, so RPPAPI and the _sv literal need <rpp/config.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "strview.h"

export module rpp.strview;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp { // from config.h, which carries no module
    using rpp::byte;
    using rpp::int16;
    using rpp::int32;
    using rpp::int64;
    using rpp::uint;
    using rpp::uint16;
    using rpp::uint32;
    using rpp::uint64;
    using rpp::ulong;
    using rpp::ushort;
}

export namespace rpp {
    using rpp::strcontains;
    using rpp::strequals;
    using rpp::strequalsi;
    using rpp::strcontainsi;
    using rpp::to_double;
    using rpp::to_int;
    using rpp::to_int64;
    using rpp::to_inthx;
    using rpp::_tostring;
    using rpp::to_string;
    using rpp::string;
    using rpp::ustring;
    using rpp::utf8len;
    using rpp::utf16len;
    using rpp::strview;
    using rpp::strview_traits;
    using rpp::StringViewType;
    using rpp::operator>>;
    using rpp::operator+=;
    using rpp::operator+;
    using rpp::concat;
    using rpp::operator<;
    using rpp::operator>;
    using rpp::operator<=;
    using rpp::operator>=;
    using rpp::operator==;
    using rpp::operator!=;
    using rpp::strview_;
    using rpp::is_likely_utf8;
    using rpp::to_lower;
    using rpp::to_upper;
    using rpp::replace;
    using rpp::line_parser;
    using rpp::keyval_parser;
    using rpp::bracket_parser;
#if RPP_ENABLE_UNICODE
    using rpp::ustrview;
    using rpp::to_ustring;
#endif
}

export namespace rpp::inline literals {
    using rpp::literals::operator""_sv;
}
// GENERATED EXPORTS END
