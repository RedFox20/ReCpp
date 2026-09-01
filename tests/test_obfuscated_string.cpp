#include <rpp/tests.h>
#include <rpp/obfuscated_string.h>
#if RPP_BUILD_WITH_MODULES
import rpp.obfuscated_string; // includes come first, the import goes last
#endif

TestImpl(test_obfuscated_string)
{
    TestInit(test_obfuscated_string)
    {
    }

    TestCase(cross_platform)
    {
        using namespace std::string_literals;
        constexpr auto str = rpp::make_obfuscated("test!1234!õäöü");
        std::string decrypted = str.to_string();
        AssertThat(decrypted, "test!1234!õäöü"s);
    }

    // the template deduces the literal length, so this API needs no macro
    TestCase(deduces_literal_length)
    {
        constexpr auto str = rpp::make_obfuscated("hello");
        static_assert(decltype(str)::length == 5, "make_obfuscated must deduce the literal length");
        AssertEqual(decltype(str)::length, 5);
        AssertEqual((int)str.obfuscated().size(), 5);
    }

    TestCase(obfuscated_text_differs_from_plaintext)
    {
        using namespace std::string_literals;
        constexpr auto str = rpp::make_obfuscated("super@secret.com");
        AssertNotEqual(str.obfuscated(), "super@secret.com"s);
        AssertEqual(str.to_string(), "super@secret.com"s);
    }

};