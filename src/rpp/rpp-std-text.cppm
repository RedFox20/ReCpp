// The std string names a public ReCpp signature writes. `rpp.std` re-exports this part.
module;

#include <string>
#include <string_view>

export module rpp.std.text;

export namespace std {
    using std::string;
    using std::wstring;
    using std::u16string;
    using std::to_string;
    using std::basic_string;
    using std::char_traits;
    using std::string_view;
    using std::wstring_view;
    using std::u16string_view; // rpp::ustrview takes one, behind its string_view_t alias

    // an exported type does not carry its free operators, so `s != "x"` fails in an
    // importer which includes no <string>
    using std::operator==;
    using std::operator!=;
    using std::operator+;
    using std::operator<=>;
}
