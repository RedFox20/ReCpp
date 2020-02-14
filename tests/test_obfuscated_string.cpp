#include <rpp/tests.h>
#include <rpp/obfuscated_string.h>
using namespace rpp;

TestImpl(test_obfuscated_string)
{
    TestInit(test_obfuscated_string)
    {
    }

    TestCase(cross_platform)
    {
        constexpr auto str = make_obfuscated("test!1234!õäöü");
        std::string decrypted = str.to_string();
        AssertThat(decrypted, "test!1234!õäöü"s);
    }

};