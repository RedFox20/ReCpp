#include <rpp/tests.h>
#if !RPP_BUILD_WITH_MODULES
#include <rpp/obfuscated_string.h>
#else
import rpp.obfuscated_string; // the module alone must carry the whole surface
#endif

TestImpl(test_obfuscated_string)
{
    TestInit(test_obfuscated_string)
    {
    }

    // scans the bytes of the object itself, which is where the plaintext would sit if it survived
    static bool holds_plaintext(const void* object, size_t size, const std::string& plaintext)
    {
        std::string_view image { static_cast<const char*>(object), size };
        return image.find(plaintext) != std::string_view::npos;
    }

    TestCase(round_trips_through_the_scrambled_bytes)
    {
        constexpr auto str = rpp::make_obfuscated("test!1234!õäöü");
        AssertEqual(str.to_string(), std::string{"test!1234!õäöü"});
    }

    TestCase(the_object_holds_no_plaintext)
    {
        static constexpr auto secret = rpp::make_obfuscated("super@secret.com");
        AssertFalse(holds_plaintext(&secret, sizeof(secret), "super@secret.com"));
        AssertEqual(secret.to_string(), std::string{"super@secret.com"});
    }

    TestCase(scrambled_bytes_differ_from_the_plaintext)
    {
        constexpr auto str = rpp::make_obfuscated("super@secret.com");
        AssertNotEqual(std::string{str.obfuscated()}, std::string{"super@secret.com"});
        AssertEqual((int)str.obfuscated().size(), 16);
    }

    // the index mixes in, so a repeated character must not repeat in the output
    TestCase(a_repeated_character_scrambles_differently)
    {
        constexpr auto str = rpp::make_obfuscated("aaaa");
        std::string_view out = str.obfuscated();
        AssertNotEqual(out[0], out[1]);
        AssertNotEqual(out[1], out[2]);
        AssertEqual(str.to_string(), std::string{"aaaa"});
    }

    // a view into a temporary dangles, and ASAN reported stack-use-after-scope before the delete
    template<class T> static constexpr bool views_an_lvalue = requires(T t) { t.obfuscated(); };
    template<class T> static constexpr bool views_a_temporary
        = requires(T t) { static_cast<T&&>(t).obfuscated(); };

    TestCase(a_view_into_a_temporary_does_not_compile)
    {
        using Obfuscated = rpp::obfuscated_string<5>;
        static_assert(views_an_lvalue<Obfuscated>, "a named object must still give a view");
        static_assert(!views_a_temporary<Obfuscated>, "a temporary must not give a view");
        AssertTrue(views_an_lvalue<Obfuscated>);
        AssertFalse(views_a_temporary<Obfuscated>);
    }

    TestCase(deduces_the_literal_length)
    {
        constexpr auto str = rpp::make_obfuscated("hello");
        static_assert(decltype(str)::length == 5, "make_obfuscated must deduce the literal length");
        AssertEqual(decltype(str)::length, 5);
    }

    TestCase(handles_the_empty_and_single_character_edges)
    {
        AssertEqual(rpp::make_obfuscated("").to_string(), std::string{});
        AssertEqual(rpp::make_obfuscated("x").to_string(), std::string{"x"});
        AssertEqual(rpp::make_obfuscated("").length, 0);
    }

    TestCase(the_literal_operator_matches_make_obfuscated)
    {
        using namespace rpp::literals;
        constexpr auto literal = "super@secret.com"_obfuscated;
        constexpr auto made = rpp::make_obfuscated("super@secret.com");
        AssertEqual(std::string{literal.obfuscated()}, std::string{made.obfuscated()});
        AssertEqual(literal.to_string(), made.to_string());
    }

    // CTAD gives the same object, so a caller needs no factory function
    TestCase(class_template_argument_deduction_works)
    {
        constexpr rpp::obfuscated_string str{"super@secret.com"};
        AssertEqual(str.length, 16);
        AssertEqual(str.to_string(), std::string{"super@secret.com"});
    }
};
