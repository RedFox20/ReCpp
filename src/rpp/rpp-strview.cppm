// C++20 Module interface unit for rpp::strview
// This wraps the existing strview.h header and re-exports its public types.
// Macros (RPPAPI, RPP_ENABLE_UNICODE, _sv literal, etc.) are NOT exported;
// users must #include <rpp/config.h> for macros.
module;

// Global module fragment: include traditional headers here.
// All declarations from these headers belong to the global module
// and are "reachable" but not "visible" to importers.
#include "strview.h"
// strview.h transitively includes: config.h, <cstring>, <string>, <concepts>

export module rpp.strview;

// Re-export the public API types from rpp::strview.h
// by making them "visible" to importers via using-declarations.
export namespace rpp {
    // config.h integer type aliases
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

    // strview.h type aliases
    using rpp::string;   // = std::string
    using rpp::ustring;  // = std::u16string

    // strview.h core types
    using rpp::strview;
#if RPP_ENABLE_UNICODE
    using rpp::ustrview;
#endif

    // strview.h parsers
    using rpp::line_parser;
    using rpp::keyval_parser;
    using rpp::bracket_parser;

    // strview.h traits and concepts
    using rpp::strview_traits;
    using rpp::StringViewType;

    // strview.h POD variant
    using rpp::strview_;
}

// strview.h literal operators
// Exported as a separate inline namespace so that
// `using namespace rpp::literals;` works for importers.
export namespace rpp::inline literals {
    using rpp::literals::operator""_sv;
#if RPP_ENABLE_UNICODE
    using rpp::literals::operator""_sv;  // char16_t overload
#endif
}

export namespace rpp {
    // strview.h constexpr strlen helpers
    using rpp::utf8len;
    using rpp::utf16len;

    // strview.h low-level string search functions
    using rpp::strcontains;
    using rpp::strcontainsi;
    using rpp::strequals;
    using rpp::strequalsi;

    // strview.h number parsing
    using rpp::to_double;
    using rpp::to_int;
    using rpp::to_int64;
    using rpp::to_inthx;

    // strview.h number-to-string (low-level itoa/ftoa)
    using rpp::_tostring;

    // strview.h UTF-8 detection
    using rpp::is_likely_utf8;

#if RPP_ENABLE_UNICODE
    // strview.h unicode conversion
    using rpp::to_string;
    using rpp::to_ustring;
#endif

    // strview.h string concat
    using rpp::concat;

    // strview.h string transform
    using rpp::to_lower;
    using rpp::to_upper;
    using rpp::replace;

    // strview.h stream operators (strview >> float/double/int/int64/bool/string)
    using rpp::operator>>;

    // strview.h concatenation operators (strview + strview, string + strview, etc.)
    using rpp::operator+;
    using rpp::operator+=;

    // strview.h comparison operators (string/const char* vs strview)
    using rpp::operator==;
    using rpp::operator!=;
    using rpp::operator<;
    using rpp::operator>;
    using rpp::operator<=;
    using rpp::operator>=;
}
