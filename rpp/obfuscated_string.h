//
//  obfuscated_string.h
//
//  Compile-time string obfuscation, Copyright (c) 2017 - Jorma Rebane
//
#ifndef RPP_OBFUSCATED_STRING_H
#define RPP_OBFUSCATED_STRING_H
#include <string>

namespace rpp
{
    using namespace std;

    static constexpr char obfuscate  (char ch, size_t i) { return (ch + i) ^ 0x55; }
    static constexpr char deobfuscate(char ch, size_t i) { return (ch ^ 0x55) - i; }
    
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
    template<char... chars> struct obfuscated_string<integer_sequence<char, chars...>> // specialize for tstring
    {
        template<size_t... index>
        static string create_obfuscated_string(index_sequence<index...>)
        {
            static constexpr char str[sizeof...(chars)] = { obfuscate(chars, index)... };
            return {str, sizeof...(chars) };
        }
        const string& obfuscated() const
        {
            static const string str = create_obfuscated_string(make_index_sequence<sizeof...(chars)>{});
            return str;
        }
        string to_string() const
        {
            string result = obfuscated();
            for (size_t i = 0; i < sizeof...(chars); ++i)
                result[i] = deobfuscate(result[i], i);
            return result;
        }
    };

#if __clang__ || __GNUC__
    template<class T, T... chars>
    constexpr auto operator""_obfuscated() { return obfuscated_string<integer_sequence<char, chars...>>{}; }
#else
    #pragma error Unsupported compiler
#endif

}

#endif /* RPP_OBFUSCATED_STRING_H */
