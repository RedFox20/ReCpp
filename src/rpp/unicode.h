#pragma once
/**
 * UTF-8 and UTF-16 code point primitives, Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 *
 * unicode.cpp defines them, and builds the to_string() and to_ustring() conversions of strview.h from them.
 */
#include "config.h" // RPPAPI

namespace rpp
{
    /**
     * @brief Decodes the UTF-8 code point at utf8[i] and moves i past it.
     * @returns The code point, or -1 for an overlong form, a surrogate, a value over U+10FFFF or a truncated sequence.
     */
    RPPAPI int utf8_decode(const char* utf8, int len, int& i) noexcept;

    /**
     * @brief Decodes the UTF-16 code point at utf16[i] and moves i past it.
     * @returns The code point, or -1 for a lone surrogate, and then i moves past that one unit.
     */
    RPPAPI int utf16_decode(const char16_t* utf16, int len, int& i) noexcept;

    /** @returns The UTF-8 byte count of a code point, from 1 to 4. */
    constexpr int utf8_size(char32_t cp) noexcept { return 1 + (cp >= 0x80) + (cp >= 0x800) + (cp >= 0x10000); }

    /** @returns The UTF-16 unit count of a code point, 1 or 2. */
    constexpr int utf16_size(char32_t cp) noexcept { return 1 + (cp >= 0x10000); }

    /**
     * @brief Writes the UTF-8 form of a code point to out. A surrogate gets its 3-byte form.
     * @returns The byte count, which is utf8_size(cp).
     */
    RPPAPI int utf8_encode(char* out, char32_t cp) noexcept;

    /**
     * @brief Writes the UTF-16 form of a code point to out, as a surrogate pair over U+FFFF.
     * @returns The unit count, which is utf16_size(cp).
     */
    RPPAPI int utf16_encode(char16_t* out, char32_t cp) noexcept;

    /**
     * @brief Writes UTF-16 from utf16[i] as UTF-8 while whole sequences fit in out_max bytes, and moves i past them.
     *        A lone surrogate gets its 3-byte form (WTF-8), so no input fails. out gets no terminator.
     * @returns The byte count.
     */
    RPPAPI int utf16_to_wtf8(char* out, int out_max, const char16_t* utf16, int len, int& i) noexcept;
} // namespace rpp
