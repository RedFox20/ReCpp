#include <rpp/tests.h>
#include <rpp/obfuscated_string.h>

TestImpl(test_obfuscated_string)
{
    TestInit(test_obfuscated_string)
    {
    }

    TestCase(cross_platform)
    {
        using namespace std::string_literals;
        constexpr auto str = make_obfuscated("test!1234!õäöü");
        std::string decrypted = str.to_string();
        AssertThat(decrypted, "test!1234!õäöü"s);
    }

};