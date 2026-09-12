// C++20 module interface unit for the rpp.text headers, owned by tools/gen_module_exports.py.
// The headers stay in the global module fragment, so an importer and an includer share one entity.
module;

#include "strview.h"
#include "sprint.h"
#include "obfuscated_string.h"

export module rpp.text;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

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
    using rpp::__wrap;
    using rpp::format_opt;
    using rpp::none;
    using rpp::lowercase;
    using rpp::uppercase;
    using rpp::string_buffer;
    using rpp::has_ostream_op;
    using rpp::has_member_sbuf_op;
    using rpp::operator<<;
    using rpp::to_hex_string;
    using rpp::print;
    using rpp::println;
    using rpp::sprint;
    using rpp::sprintln;
    using rpp::format;
    using rpp::obfuscated_string;
    using rpp::make_obfuscated;
#if RPP_ENABLE_UNICODE
    using rpp::ustrview;
    using rpp::to_ustring;
#endif
}

export namespace rpp::inline literals {
    using rpp::literals::operator""_sv;
    using rpp::literals::operator""_obfuscated;
}
// GENERATED EXPORTS END
