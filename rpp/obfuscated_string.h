//
//  obfuscated_string.h
//
//  Compile-time string obfuscation, Copyright (c) 2017 - Jorma Rebane
//
#ifndef RPP_OBFUSCATED_STRING_H
#define RPP_OBFUSCATED_STRING_H
#include <string>
#include <utility>

namespace rpp
{
    using namespace std;

    static constexpr char obfuscate  (char ch, int i) { return char((ch + i) ^ 0x55); }
    static constexpr char deobfuscate(char ch, int i) { return char((ch ^ 0x55) - i); }

    template<int... i>  using int32_sequence = integer_sequence<int, i...>;
    template<int count> using int32_indices  = make_integer_sequence<int, count>;



    template<class T> struct obfuscated_string // unspecialized dummy
    {
    };

    /**
     * Compile-time obfuscated string to prevent easy decompilation of app strings
     * This will greatly reduce the possibility of reverse engineering stored URL's
     *
     * Note:
     *   Works in both Debug and Release builds
     *   This is a C++14 GNU extension available with GCC and clang
     *
     * Usage:
     *   constexpr auto email = "super@secret.com"_obfuscated;
     *   
     *   cout << "Obfuscated:   '" << email.obfuscated() << "'\n";
     *   cout << "Deobfuscated: '" << email.to_string()  << "'\n";
     *
     * Result:
     *   Obfuscated:   '&#'=#,9>.:*o%()'
     *   Deobfuscated: 'super@secret.com'
     */
    template<char... chars>
    struct obfuscated_string<integer_sequence<char, chars...>> // specialize for tstring
    {
        static constexpr int length = sizeof...(chars);
        template<int... index> static string create_obfuscated_string(int32_sequence<index...>)
        {
            static constexpr char str[length] = { obfuscate(chars, index)... };
            return {str, str + length };
        }
        const string& obfuscated() const
        {
            static const string str = create_obfuscated_string(int32_indices<sizeof...(chars)>{});
            return str;
        }
        string to_string() const
        {
            string result = obfuscated();
            for (int i = 0; i < sizeof...(chars); ++i)
                result[i] = deobfuscate(result[i], i);
            return result;
        }
    };



    template<class T> struct macro_obfuscated_string // unspecialized dummy
    {
    };

    /**
     * @brief Since MSVC++ does not implement 'template<class T, T... chars>' special
     * literal extension, we need another type to expand from macros.
     * So if you need cross-platform support, please use the below syntax:
     * 
     * Usage:
     *   constexpr auto email = make_obfuscated("super@secret.com");
     *   
     *   cout << "Obfuscated:   '" << email.obfuscated() << "'\n";
     *   cout << "Deobfuscated: '" << email.to_string()  << "'\n";
     * 
     * Result:
     *   Obfuscated:   '&#'=#,9>.:*o%()'
     *   Deobfuscated: 'super@secret.com'
     */
    template<int... index>
    struct macro_obfuscated_string<int32_sequence<index...>> 
    {
        static constexpr int length = sizeof...(index);
        char chars[length];
        constexpr macro_obfuscated_string(const char* const str) 
            : chars{ obfuscate(str[index], index)... } {}
        string obfuscated() const
        {
            return { chars, chars + length };
        }
        string to_string() const
        {
            string result = obfuscated();
            for (int i = 0; i < length; ++i)
                result[i] = deobfuscate(result[i], i);
            return result;
        }
    };



#if __clang__ || __GNUC__

#if __GNUC__ && !__clang__
    template<class T, T... chars> constexpr auto operator""_obfuscated()
    {
        return obfuscated_string<integer_sequence<char, chars...>>{};
    }
#endif

    #define make_obfuscated(str) rpp::macro_obfuscated_string<rpp::int32_indices<sizeof(str)-1>>(str)

#elif _MSC_VER

    //template<char... chars> constexpr auto operator"" _obfuscated()
    //{
    //    return obfuscated_string<integer_sequence<char, chars...>>{};
    //}

    #define make_obfuscated(str) rpp::macro_obfuscated_string<rpp::int32_indices<sizeof(str)-1>>{str}

#else
    static_assert(false, "obfuscated_string not yet supported on this compiler");
#endif

}

#endif // RPP_OBFUSCATED_STRING_H
