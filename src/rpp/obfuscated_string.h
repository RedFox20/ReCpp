#pragma once
/**
 * Compile-time string obfuscation, Copyright (c) 2017, Jorma Rebane
 * Distributed under MIT Software License
 */
#include <string>      // std::string
#include <string_view> // std::string_view

namespace rpp
{
    /**
     * @brief A string literal scrambled at compile time, so the plaintext never becomes static data.
     *
     * The constructor is consteval, so only the scrambled bytes reach the binary. Each byte mixes in
     * its own index, so a repeated character does not repeat in the output.
     *
     * Usage:
     *   using namespace rpp::literals;
     *   constexpr auto email = "super@secret.com"_obfuscated;
     *   std::string plain = email.to_string();
     *
     * @warning This defeats a strings dump of the binary. It is not encryption, and anyone who
     *          runs the program under a debugger reads the plaintext out of to_string().
     */
    // N is the array size of the literal, so the constructor deduces it with no deduction guide,
    // which a module cannot export
    template<int N>
    struct obfuscated_string
    {
        static constexpr int length = N - 1;

        char chars[N]; // public, so a test can read the object representation

        consteval obfuscated_string(const char (&str)[N]) noexcept : chars{}
        {
            for (int i = 0; i < length; ++i) chars[i] = scramble(str[i], i);
        }

        /// @returns the scrambled bytes, which never match the plaintext
        constexpr std::string_view obfuscated() const & noexcept { return { chars, length }; }

        /// The view would outlive a temporary, so this call does not compile. Name the object first.
        void obfuscated() const && = delete;

        /// @returns the plaintext, restored into a fresh string
        std::string to_string() const
        {
            // volatile forces a real load, so the optimizer cannot fold the plaintext back into the binary
            const volatile char* src = chars;
            std::string out(length, '\0');
            for (int i = 0; i < length; ++i) out[i] = unscramble(src[i], i);
            return out;
        }

    private:
        // the index mixes in, so a repeated character does not repeat in the output
        static constexpr char scramble(char ch, int i) noexcept { return char((ch + i) ^ 0x55); }
        static constexpr char unscramble(char ch, int i) noexcept { return char((ch ^ 0x55) - i); }
    };

    /// Creates a compile-time obfuscated string from a string literal.
    template<int N>
    consteval obfuscated_string<N> make_obfuscated(const char (&str)[N]) noexcept { return { str }; }

    inline namespace literals
    {
        /// Creates a compile-time obfuscated string: `constexpr auto s = "text"_obfuscated;`
        template<obfuscated_string S> constexpr auto operator ""_obfuscated() noexcept { return S; }
    }
}
