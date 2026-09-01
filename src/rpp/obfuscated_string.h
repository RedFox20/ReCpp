#pragma once
/**
 * Compile-time string obfuscation, Copyright (c) 2017, Jorma Rebane
 * Distributed under MIT Software License
 */
#include <string>
#include <utility>

namespace rpp
{
    static constexpr char obfuscate  (char ch, int i) { return char((ch + i) ^ 0x55); }
    static constexpr char deobfuscate(char ch, int i) { return char((ch ^ 0x55) - i); }

    template<int... i>  using int32_sequence = std::integer_sequence<int, i...>;
    template<int count> using int32_indices  = std::make_integer_sequence<int, count>;



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
    struct obfuscated_string<std::integer_sequence<char, chars...>> // specialize for tstring
    {
        static constexpr int length = sizeof...(chars);
        template<int... index> static std::string create_obfuscated_string(int32_sequence<index...>)
        {
            static constexpr char str[length] = { obfuscate(chars, index)... };
            return {str, str + length };
        }
        const std::string& obfuscated() const
        {
            static const std::string str = create_obfuscated_string(int32_indices<sizeof...(chars)>{});
            return str;
        }
        std::string to_string() const
        {
            std::string result = obfuscated();
            for (int i = 0; i < length; ++i)
                result[i] = deobfuscate(result[i], i);
            return result;
        }
    };



    template<class T> struct macro_obfuscated_string // unspecialized dummy
    {
    };

    /**
     * @brief The cross-platform obfuscated string, built by rpp::make_obfuscated().
     * MSVC++ does not implement the 'template<class T, T... chars>' literal extension.
     *
     * Usage:
     *   constexpr auto email = rpp::make_obfuscated("super@secret.com");
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
        std::string obfuscated() const
        {
            return { chars, chars + length };
        }
        std::string to_string() const
        {
            std::string result = obfuscated();
            for (int i = 0; i < length; ++i)
                result[i] = deobfuscate(result[i], i);
            return result;
        }
    };



    /// Creates a compile-time obfuscated string from a string literal.
    /// The array reference deduces the length, so this needs no macro.
    template<int N>
    constexpr macro_obfuscated_string<int32_indices<N-1>> make_obfuscated(const char (&str)[N])
    {
        return macro_obfuscated_string<int32_indices<N-1>>{ str };
    }

#if __GNUC__ && !__clang__
    template<class T, T... chars> constexpr auto operator ""_obfuscated()
    {
        return obfuscated_string<std::integer_sequence<char, chars...>>{};
    }
#endif

}
