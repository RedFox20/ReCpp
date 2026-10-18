/**
 * UTF-8 and UTF-16 code point primitives and the conversions of strview.h, Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "unicode.h"
#include "strview.h" // rpp::to_string, rpp::to_ustring
#include <cstddef> // ptrdiff_t
#include <cstdint> // SIZE_MAX
#include <cstring> // strlen
#include <string> // std::char_traits

namespace rpp
{
    // The conversion loops take these inline. The functions of unicode.h call them.
    static inline int decode_utf8(const char* utf8, int len, int& i) noexcept
    {
        static constexpr int MIN_CODE_POINT[] = { 0, 0x80, 0x800, 0x10000 };
        int cp = static_cast<unsigned char>(utf8[i++]);
        if (cp < 0x80) return cp;
        if (cp < 0xC2 || cp > 0xF4) return -1; // A continuation byte, an overlong lead or a lead over U+10FFFF is invalid.
        const int tail = 1 + (cp >= 0xE0) + (cp >= 0xF0);
        if (len - i < tail) return -1;
        cp &= 0x3F >> tail;
        for (const int end = i + tail; i < end; ++i)
        {
            const int byte = static_cast<unsigned char>(utf8[i]);
            if ((byte & 0xC0) != 0x80) return -1;
            cp = (cp << 6) | (byte & 0x3F);
        }
        if (cp < MIN_CODE_POINT[tail] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return -1;
        return cp;
    }

    static inline int decode_utf16(const char16_t* utf16, int len, int& i) noexcept
    {
        const int unit = utf16[i++];
        if (unit < 0xD800 || unit > 0xDFFF) return unit;
        const int low = i < len ? utf16[i] : 0;
        if (unit > 0xDBFF || low < 0xDC00 || low > 0xDFFF) return -1;
        ++i;
        return 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
    }

    static inline int encode_utf8(char* out, char32_t cp) noexcept
    {
        if (cp < 0x80)
        {
            out[0] = static_cast<char>(cp);
            return 1;
        }
        static constexpr char32_t LEAD_BITS[] = { 0x00, 0xC0, 0xE0, 0xF0 };
        const int tail = utf8_size(cp) - 1;
        out[0] = static_cast<char>(LEAD_BITS[tail] | (cp >> (6 * tail)));
        for (int n = 1; n <= tail; ++n)
            out[n] = static_cast<char>(0x80 | ((cp >> (6 * (tail - n))) & 0x3F));
        return tail + 1;
    }

    static inline int encode_utf16(char16_t* out, char32_t cp) noexcept
    {
        if (cp < 0x10000)
        {
            out[0] = static_cast<char16_t>(cp);
            return 1;
        }
        out[0] = static_cast<char16_t>(0xD800 + ((cp - 0x10000) >> 10));
        out[1] = static_cast<char16_t>(0xDC00 + (cp & 0x3FF));
        return 2;
    }

    int utf8_decode(const char* utf8, int len, int& i) noexcept { return decode_utf8(utf8, len, i); }
    int utf16_decode(const char16_t* utf16, int len, int& i) noexcept { return decode_utf16(utf16, len, i); }
    int utf8_encode(char* out, char32_t cp) noexcept { return encode_utf8(out, cp); }
    int utf16_encode(char16_t* out, char32_t cp) noexcept { return encode_utf16(out, cp); }

    int utf16_to_wtf8(char* out, int out_max, const char16_t* utf16, int len, int& i) noexcept
    {
        int n = 0;
        while (i < len)
        {
            const int start = i;
            const int cp = decode_utf16(utf16, len, i);
            const char32_t written = cp < 0 ? utf16[start] : static_cast<char32_t>(cp); // A lone surrogate keeps its 3-byte form.
            if (out_max - n < utf8_size(written))
            {
                i = start;
                break;
            }
            n += encode_utf8(out + n, written);
        }
        return n;
    }

#if RPP_ENABLE_UNICODE
    // Writes whole UTF-8 sequences while they fit in room bytes, and returns their size, or -1 for invalid input.
    // The room is a size_t, because the result of a string form can be longer than the int out_max of a buffer form.
    static ptrdiff_t utf16_to_utf8(char* out, size_t room, const char16_t* utf16, int utf16len) noexcept
    {
        size_t n = 0;
        for (int i = 0; i < utf16len;)
        {
            const int cp = decode_utf16(utf16, utf16len, i);
            if (cp < 0) return -1;
            if (room - n < static_cast<size_t>(utf8_size(cp))) break;
            n += static_cast<size_t>(encode_utf8(out + n, cp));
        }
        return static_cast<ptrdiff_t>(n);
    }

    // Writes whole code points while they fit in room units, and returns the unit count, or -1 for invalid input.
    static ptrdiff_t utf8_to_utf16(char16_t* out, size_t room, const char* utf8, int utf8len) noexcept
    {
        size_t n = 0;
        for (int i = 0; i < utf8len;)
        {
            const int cp = decode_utf8(utf8, utf8len, i);
            if (cp < 0) return -1;
            if (room - n < static_cast<size_t>(utf16_size(cp))) break;
            n += static_cast<size_t>(encode_utf16(out + n, cp));
        }
        return static_cast<ptrdiff_t>(n);
    }

    string to_string(const char16_t* utf16, int utf16len) noexcept
    {
        if (utf16len < 0) utf16len = static_cast<int>(std::char_traits<char16_t>::length(utf16));
        // Over SIZE_MAX / 3 units, the input and its UTF-8 cannot both fit in the address space of a 32-bit target.
        if (static_cast<size_t>(utf16len) > SIZE_MAX / 3) return {};
        string ret(static_cast<size_t>(utf16len) * 3, '\0'); // A UTF-16 unit never takes more than 3 UTF-8 bytes.
        const ptrdiff_t n = utf16_to_utf8(ret.data(), ret.size(), utf16, utf16len);
        ret.resize(n < 0 ? 0 : static_cast<size_t>(n)); // Invalid input gives an empty string.
        return ret;
    }

    int to_string(char* out, int out_max, const char16_t* utf16, int utf16len) noexcept
    {
        if (out_max <= 0) return -1; // The buffer has no room for the terminator.
        if (utf16len < 0) utf16len = static_cast<int>(std::char_traits<char16_t>::length(utf16));
        const ptrdiff_t n = utf16_to_utf8(out, static_cast<size_t>(out_max) - 1, utf16, utf16len);
        out[n < 0 ? 0 : n] = '\0'; // Invalid input gives an empty string.
        return static_cast<int>(n);
    }

    ustring to_ustring(const char* utf8, int utf8len) noexcept
    {
        if (utf8len < 0) utf8len = int(strlen(utf8));
        ustring ret(static_cast<size_t>(utf8len), u'\0'); // A UTF-8 sequence never gives more UTF-16 units than it has bytes.
        const ptrdiff_t n = utf8_to_utf16(ret.data(), ret.size(), utf8, utf8len);
        ret.resize(n < 0 ? 0 : static_cast<size_t>(n)); // Invalid input gives an empty string.
        return ret;
    }

    int to_ustring(char16_t* out, int out_max, const char* utf8, int utf8len) noexcept
    {
        if (out_max <= 0) return -1; // The buffer has no room for the terminator.
        if (utf8len < 0) utf8len = int(strlen(utf8));
        const ptrdiff_t n = utf8_to_utf16(out, static_cast<size_t>(out_max) - 1, utf8, utf8len);
        out[n < 0 ? 0 : n] = u'\0'; // Invalid input gives an empty string.
        return static_cast<int>(n);
    }
#endif // RPP_ENABLE_UNICODE
} // namespace rpp
