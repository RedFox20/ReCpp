/**
 * String Tokenizer/View, Copyright (c) 2014 - 2018, Jorma Rebane
 */
// ReSharper disable IdentifierTypo
// ReSharper disable CommentTypo
#include "strview.h"
#include <math.h> // use math.h for GCC compatibility  // NOLINT
#include <cstdlib>
#include <cstring> // memcpy
#include <locale> // toupper
#include <cfloat> // DBL_MAX
//#include <charconv> // to_chars, C++17, not implemented yet
#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#else
    #define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING 1
    #include <codecvt> // codecvt_utf8
#endif

namespace rpp
{
    // This is same as memchr, but optimized for very small control strings
    bool strcontains(const char* str, int len, char ch) noexcept {
        for (int i = 0; i < len; ++i) if (str[i] == ch) return true; // found it.
        return false;
    }
    bool strcontainsi(const char* str, int len, char ch) noexcept {
        for (int i = 0; i < len; ++i) if (::toupper(str[i]) == ::toupper(ch)) return true; // found it.
        return false;
    }
    const char* strfindi(const char* str, int len, char ch) noexcept {
        for (int i = 0; i < len; ++i) if (::toupper(str[i]) == ::toupper(ch)) return &str[i]; // found it.
        return nullptr;
    }
    /**
     * @note Same as strpbrk, except we're not dealing with 0-term strings
     * @note This function is optimized for 4-8 char str and 3-4 char control.
     */
    const char* strcontains(const char* str, int nstr, const char* control, int ncontrol) noexcept {
        for (auto s = str; nstr; --nstr, ++s)
            if (strcontains(control, ncontrol, *s)) return s; // done
        return nullptr; // not found
    }
    bool strequals(const char* s1, const char* s2, int len) noexcept {
        for (int i = 0; i < len; ++i) 
            if (s1[i] != s2[i]) return false; // not equal.
        return true;
    }
    bool strequalsi(const char* s1, const char* s2, int len) noexcept {
        for (int i = 0; i < len; ++i) 
            if (::toupper(s1[i]) != ::toupper(s2[i])) return false; // not equal.
        return true;
    }


    ///////////// string view
    
    
    const char* strview::to_cstr(char* buf, int max) const noexcept
    {
        if (str[len] == '\0')
            return str;
        auto n = size_t((len < max) ? len : max - 1);
        memcpy(buf, str, n);
        buf[n] = '\0';
        return buf;
    }

    struct ringbuf {
        static constexpr int max = 1024;
        int pos;
        char buf[max];
        struct chunk {
            char* ptr;
            int len;
        };
        chunk next(int n) {
            int rem = max - pos;
            if (rem < n) {
                pos = 0;
                rem = max;
                if (rem < n) n = rem;
            }
            char* ptr = buf+pos;
            pos += n;
            return { ptr, n };
        }
    };

    const char* strview::to_cstr() const noexcept
    {
        if (str[len] == '\0')
            return str;
        static thread_local ringbuf ringbuf;
        auto chunk = ringbuf.next(len+1);
        return to_cstr(chunk.ptr, chunk.len);
    }

    bool strview::to_bool() const noexcept
    {
        return strequalsi(str, "true") ||
               strequalsi(str, "yes")  ||
               strequalsi(str, "on")   ||
               strequalsi(str, "1");
    }

    bool strview::is_whitespace() const noexcept
    {
        auto* s = str, *e = s+len;
        for (; s < e && *const_cast<char*>(s) <= ' '; ++s) {} // loop while is whitespace
        return s == e;
    }
    
    int strview::compare(const char* s, int n) const noexcept
    {
        int len1 = len;
        int len2 = n;
        if (int res = memcmp(str, s, size_t(len1 < len2 ? len1 : len2)))
            return res;
        if (len1 < len2) return -1;
        if (len1 > len2) return +1;
        return 0;
    }
    
    int strview::compare(const char* s) const noexcept
    {
        const char* s1 = str;
        const char* s2 = s;
        for (int len1 = len; len1 > 0; --len1, ++s1, ++s2)
        {
            char c1 = *s1, c2 = *s2;
            if (c1 < c2) return -1;
            if (c1 > c2) return +1;
        }
        return 0;
    }

    strview& strview::trim_start() noexcept
    {
        auto s = str;
        auto n = len;
        for (; n && *const_cast<char*>(s) <= ' '; ++s, --n) {} // loop while is whitespace
        str = s; len = n; // result writeout
        return *this;
    }

    strview& strview::trim_start(const char ch) noexcept
    {
        auto s = str;
        auto n = len;
        for (; n && *s == ch; ++s, --n) {}
        str = s; len = n; // result writeout
        return *this;
    }

    strview& strview::trim_start(const char* chars, int nchars) noexcept
    {
        auto s = str;
        auto n = len;
        for (; n && strcontains(chars, nchars, *s); ++s, --n) {}
        str = s; len = n;
        return *this;
    }

    strview& strview::trim_end() noexcept
    {
        auto n = len;
        auto e = str + n;
        for (; n && *--e <= ' '; --n) {} // reverse loop while is whitespace
        len = n;
        return *this;
    }

    strview& strview::trim_end(char ch) noexcept
    {
        auto n = len;
        auto e = str + n;
        for (; n && *--e == ch; --n) {}
        len = n;
        return *this;
    }

    strview& strview::trim_end(const char* chars, int nchars) noexcept
    {
        auto n = len;
        auto e = str + n;
        for (; n && strcontains(chars, nchars, *--e); --n) {}
        len = n;
        return *this;
    }

    const char* strview::find(const char* substr, int sublen) const noexcept
    {
        if (size_t n = sublen) //// @note lots of micro-optimization here
        {
            const char* needle = substr;
            const char* haystr = str;
            const char* hayend = haystr + len;
            int firstChar = *needle;
            while (haystr < hayend)
            {
                haystr = static_cast<const char*>(memchr(haystr, firstChar, hayend - haystr));
                if (!haystr) 
                    return nullptr; // definitely not found

                size_t haylen = hayend - haystr;
                if (haylen >= n && memcmp(haystr, needle, n) == 0)
                    return haystr; // it's a match
                ++haystr; // no match, reset search from next char
            }
        }
        return nullptr;
    }

    const char* strview::find_icase(const char* substr, int sublen) const noexcept
    {
        if (sublen) //// @note lots of micro-optimization here
        {
            const char* needle = substr;
            const char* haystr = str;
            const char* hayend = haystr + len;
            int firstChar = *needle;
            while (haystr < hayend)
            {
                haystr = static_cast<const char*>(strfindi(haystr, int(hayend - haystr), firstChar));
                if (!haystr) 
                    return nullptr; // definitely not found

                int haylen = hayend - haystr;
                if (haylen >= sublen && strequalsi(haystr, needle, sublen))
                    return haystr; // it's a match
                ++haystr; // no match, reset search from next char
            }
        }
        return nullptr;
    }

    const char* strview::rfind(char c) const noexcept
    {
        if (len <= 0) return nullptr;
        const char* p = str;
        const char* e = p + len - 1;
        for (; e >= p; --e) if (*e == c) return e;
        return nullptr;
    }

    const char* strview::findany(const char* chars, int n) const noexcept
    {
        const char* p = str;
        const char* e = p + len;
        for (; p < e; ++p) if (strcontains(chars, n, *p)) return p;
        return nullptr;
    }

    const char* strview::rfindany(const char* chars, int n) const noexcept
    {
        if (len <= 0) return nullptr;
        const char* p = str;
        const char* e = p + len - 1;
        for (; e >= p; --e) if (strcontains(chars, n, *e)) return e;
        return nullptr;
    }

    int strview::count(char ch) const noexcept
    {
        int count = 0;
        for (auto* p = str, *e = str+len; p < e; ++p)
            if (*p == ch) ++count;
        return count;
    }

    int strview::indexof(char ch) const noexcept
    {
        for (int i = 0; i < len; ++i)
            if (str[i] == ch) return i;
        return -1;
    }

    int strview::rindexof(char ch) const noexcept
    {
        for (int i = len-1; i >= 0; --i)
            if (str[i] == ch) return i;
        return -1;
    }

    int strview::indexofany(const char* chars, int n) const noexcept
    {
        for (int i = 0; i < len; ++i)
            if (strcontains(chars, n, str[i])) return i;
        return -1;
    }

    strview strview::split_first(char delim) const noexcept
    {
        if (auto splitEnd = static_cast<const char*>(memchr(str, delim, size_t(len))))
            return strview(str, splitEnd); // if we find a separator, readjust end of strview to that
        return strview(str, len);
    }

    strview strview::split_first(const char* substr, int sublen) const noexcept
    {
        int l = len;
        const char* s = str;
        const char* e = s + l;
        char ch = *substr;
        while ((s = static_cast<const char*>(memchr(s, ch, size_t(l)))) != nullptr) {
            l = int(e - s);
            if (l >= sublen && strequals(s, substr, sublen)) {
                return strview(str, s);
            }
            --l; ++s;
        }
        return strview(str, len);
    }

    strview strview::split_second(char delim) const noexcept
    {
        if (auto splitstart = static_cast<const char*>(memchr(str, delim, size_t(len))))
            return strview(splitstart + 1, str+len); // readjust start, also skip the char we split at
        return strview(str, len);
    }




    bool strview::next(strview& out, char delim) noexcept
    {
        return _next_trim(out, [delim](const char* s, int n) {
            return static_cast<const char*>(memchr(s, delim, size_t(n)));
        });
    }

    bool strview::next(strview& out, const char* delims, int ndelims) noexcept
    {
        return _next_trim(out, [delims, ndelims](const char* s, int n) {
            return strcontains(s, n, delims, ndelims);
        });
    }





    bool strview::next_notrim(strview& out, char delim) noexcept
    {
        return _next_notrim(out, [delim](const char* s, int n) {
            return static_cast<const char*>(memchr(s, delim, size_t(n)));
        });
    }

    bool strview::next_notrim(strview& out, const char* delims, int ndelims) noexcept
    {
        return _next_notrim(out, [delims, ndelims](const char* s, int n) {
            return strcontains(s, n, delims, ndelims);
        });
    }


    strview strview::substr(int index, int length) const noexcept
    {
        int idx = index;
        if (idx > len)    idx = len;
        else if (idx < 0) idx = 0;
        
        int remaining = len - idx;
        if (remaining > length)
            remaining = length;

        return { str + idx, remaining };
    }

    strview strview::substr(int index) const noexcept
    {
        int idx = index;
        if (idx > len)    idx = len;
        else if (idx < 0) idx = 0;

        int remaining = len - idx;
        return { str + idx, remaining };
    }


    double strview::next_double() noexcept
    {
        auto s = str, e = s + len;
        for (; s < e; ++s) {
            char ch = *s; // check if we have the start of a number: "-0.25" || ".25" || "25":
            if (ch == '-' || ch == '.' || ('0' <= ch && ch <= '9')) {
                double f = rpp::to_double(s, &s);
                str = s; len = int(e - s);
                return f;
            }
        }
        str = s; len = int(e - s);
        return 0.0;
    }

    int strview::next_int() noexcept
    {
        auto s = str, e = s + len;
        for (; s < e; ++s) {
            char ch = *s; // check if we have the start of a number: "-25" || "25":
            if (ch == '-' || ('0' <= ch && ch <= '9')) {
                int i = rpp::to_int(s, &s);
                str = s; len = int(e - s);
                return i;
            }
        }
        str = s; len = int(e - s);
        return 0;
    }

    int64 strview::next_int64() noexcept
    {
        auto s = str, e = s + len;
        for (; s < e; ++s) {
            char ch = *s; // check if we have the start of a number: "-25" || "25":
            if (ch == '-' || ('0' <= ch && ch <= '9')) {
                int64 i = rpp::to_int64(s, &s);
                str = s; len = int(e - s);
                return i;
            }
        }
        str = s; len = int(e - s);
        return 0;
    }

    NOINLINE strview& strview::skip(int nchars) noexcept
    {
        auto s = str;
        auto l = len, n = nchars;
        for (; l && n > 0; --l, --n, ++s) {}
        str = s; len = l;
        return *this;
    }

    strview& strview::skip_until(char ch) noexcept
    {
        auto s = str;
        auto n = len;
        for (; n && *s != ch; --n, ++s) {}
        str = s; len = n;
        return *this;
    }
    strview& strview::skip_until(const char* substr, int sublen) noexcept
    {
        auto s = str, e = s + len;
        char ch = *substr; // starting char of the sequence
        while (s < e) {
            if (auto p = static_cast<const char*>(memchr(s, ch, e - s))) {
                if (memcmp(p, substr, size_t(sublen)) == 0) { // match found
                    str = p; len = int(e - p);
                    return *this;
                }
                s = p + 1;
                continue;
            }
            break;
        }
        str = e; len = 0;
        return *this;
    }

    strview& strview::skip_after(char ch) noexcept
    {
        skip_until(ch);
        if (len) { ++str; --len; } // skip after
        return *this;
    }
    strview& strview::skip_after(const char* substr, int sublen) noexcept
    {
        skip_until(substr, sublen);
        if (len) { str += sublen; len -= sublen; } // skip after
        return *this;
    }

    static void transform_chars(char* str, int len, int func(int)) noexcept
    {
        auto s = str, e = s + len;
        for (; s < e; ++s)
            *s = static_cast<char>(func(*s));
    }
    strview& strview::to_lower() noexcept
    {
        transform_chars(const_cast<char*>(str), len, ::tolower); return *this;
    }
    strview& strview::to_upper() noexcept
    {
        transform_chars(const_cast<char*>(str), len, ::toupper); return *this;
    }
    char* strview::as_lower(char* dst) const noexcept {
        auto p = dst;
        auto s = str;
        for (int n = len; n > 0; --n)
            *p++ = static_cast<char>(::tolower(*s++));
        *p = 0;
        return dst;
    }
    char* strview::as_upper(char* dst) const noexcept
    {
        auto p = dst;
        auto s = str;
        for (int n = len; n > 0; --n)
            *p++ = static_cast<char>(::toupper(*s++));
        *p = 0;
        return dst;
    }
    std::string strview::as_lower() const noexcept
    {
        std::string ret;
        ret.reserve(size_t(len));
        auto s = const_cast<char*>(str);
        for (int n = len; n > 0; --n)
            ret.push_back(static_cast<char>(::tolower(*s++)));
        return ret;
    }
    std::string strview::as_upper() const noexcept
    {
        std::string ret;
        ret.reserve(size_t(len));
        auto s = const_cast<char*>(str);
        for (int n = len; n > 0; --n)
            ret.push_back(static_cast<char>(::toupper(*s++)));
        return ret;
    }

    ///////////////////////////////////////////////////////////////////////////////////////

    std::string concat(const strview& a, const strview& b)
    {
        std::string str;
        auto sa = size_t(a.len), sb = size_t(b.len);
        str.reserve(sa + sb);
        str.append(a.str, sa).append(b.str, sb);
        return str;
    }
    std::string concat(const strview& a, const strview& b, const strview& c)
    {
        std::string str;
        auto sa = size_t(a.len), sb = size_t(b.len), sc = size_t(c.len);
        str.reserve(sa + sb + sc);
        str.append(a.str, sa).append(b.str, sb).append(c.str, sc);
        return str;
    }
    std::string concat(const strview& a, const strview& b, const strview& c, const strview& d)
    {
        std::string str;
        auto sa = size_t(a.len), sb = size_t(b.len), sc = size_t(c.len), sd = size_t(d.len);
        str.reserve(sa + sb + sc + sd);
        str.append(a.str, sa).append(b.str, sb).append(c.str, sc).append(d.str, sd);
        return str;
    }
    std::string concat(const strview& a, const strview& b, const strview& c, const strview& d, const strview& e)
    {
        std::string str;
        auto sa = size_t(a.len), sb = size_t(b.len), sc = size_t(c.len), sd = size_t(d.len), se = size_t(e.len);
        str.reserve(sa + sb + sc + sd + se);
        str.append(a.str, sa).append(b.str, sb).append(c.str, sc).append(d.str, sd).append(e.str, se);
        return str;
    }

    ///////////////////////////////////////////////////////////////////////////////////////

    char* to_lower(char* str, int len) noexcept
    {
        transform_chars(str, len, ::tolower); return str;
    }
    char* to_upper(char* str, int len) noexcept
    {
        transform_chars(str, len, ::toupper); return str;
    }
    std::string& to_lower(std::string& str) noexcept
    {
        transform_chars(const_cast<char*>(str.data()), static_cast<int>(str.size()), ::tolower); return str;
    }
    std::string& to_upper(std::string& str) noexcept
    {
        transform_chars(const_cast<char*>(str.data()), static_cast<int>(str.size()), ::toupper); return str;
    }

    char* replace(char* str, int len, char chOld, char chNew) noexcept
    {
        char* s = str, *e = s + len;
        for (; s < e; ++s) if (*s == chOld) *s = chNew;
        return str;
    }
    std::string& replace(std::string& str, char chOld, char chNew) noexcept {
        replace(const_cast<char*>(str.data()), static_cast<int>(str.size()), chOld, chNew); return str;
    }


    strview& strview::replace(char chOld, char chNew) noexcept
    {
        auto s = const_cast<char*>(str);
        for (int n = len; n > 0; --n, ++s)
            if (*s == chOld) *s = chNew;
        return *this;
    }

    ////////////////////// loose utility functions

    /**
     * @note Doesn't account for special cases like +- NAN. Does support scientific notation, ex: '1.0e+003' 
     *       Some optimizations applied to avoid too many multiplications
     *       This doesn't handle very big floats, max range is:
     *           -2147483648.0f to 2147483647.0f, or -0.0000000001f to 0.0000000001f
     *       Or the worst case:
     *           214748.0001f
     */
    double to_double(const char* str, int len, const char** end) noexcept
    {
        const char* s = str;
        const char* e = str + len;
        int64 power   = 1;
        int64 intPart = abs(to_int64(s, len, &s));
        char ch       = *s;
        int exponent  = 0;

        // if input is -0.xxx then intPart will lose the sign info
        // intpart growth will also break if we use neg ints
        double sign = (*str == '-') ? -1.0 : +1.0;

        // @note The '.' is actually the sole reason for this function in the first place. Locale independence.
        if (ch == '.') { /* fraction part follows*/
            while (++s < e) {
                ch = *s;
                if (ch == 'e') { // parse e-016 e+3 etc.
                    ++s;
                    int esign = (*s++ == '+') ? +1 : -1;
                    exponent = (*s++ - '0');
                    exponent = (exponent << 3) + (exponent << 1) + (*s++ - '0');
                    exponent = (exponent << 3) + (exponent << 1) + (*s++ - '0');
                    exponent *= esign;
                    break;
                }
                if (ch < '0' || '9' < ch)
                    break;
                intPart = (intPart << 3) + (intPart << 1) + int64(ch - '0'); // intPart = intPart*10 + digit
                power   = (power   << 3) + (power   << 1);                     // power *= 10
            }
        }
        if (end) *end = s; // write end of parsed value
        double result = power == 1 ? double(intPart) : double(intPart) / double(power);
        if (exponent)
            result *= pow(10.0, (double)exponent);
        return result * sign;
    }

    // optimized for simplicity and performance
    int to_int(const char* str, int len, const char** end) noexcept
    {
        const char* s = str;
        const char* e = str + len;
        int  intPart  = 0;
        bool negative = false;
        char ch       = *s;

        if (ch == '-')
        { negative = true; ++s; } // change sign and skip '-'
        else if (ch == '+')  ++s; // ignore '+'

        for (; s < e && '0' <= (ch = *s) && ch <= '9'; ++s) {
            intPart = (intPart << 3) + (intPart << 1) + (ch - '0'); // intPart = intPart*10 + digit
        }
        if (negative) intPart = -intPart; // twiddle sign

        if (end) *end = s; // write end of parsed value
        return intPart;
    }

    // optimized for simplicity and performance
    int64 to_int64(const char* str, int len, const char** end) noexcept
    {
        const char* s = str;
        const char* e = str + len;
        int64 intPart = 0;
        bool negative = false;
        char ch       = *s;

        if (ch == '-')
        { negative = true; ++s; } // change sign and skip '-'
        else if (ch == '+')  ++s; // ignore '+'

        for (; s < e && '0' <= (ch = *s) && ch <= '9'; ++s) {
            intPart = (intPart << 3) + (intPart << 1) + int64(ch - '0'); // intPart = intPart*10 + digit
        }
        if (negative) intPart = -intPart; // twiddle sign

        if (end) *end = s; // write end of parsed value
        return intPart;
    }

    // optimized for simplicity and performance
    // detects HEX integer strings as 0xBA or 0BA. Regular integers also parsed
    int to_inthx(const char* str, int len, const char** end) noexcept
    {
        const char* s = str;
        const char* e = str + len;
        unsigned intPart = 0;
        if (s[0] == '0' && s[1] == 'x') s += 2;
        for (char ch; s < e; ++s) {
            int digit;
            ch = *s;
            if      ('0' <= ch && ch <= '9') digit = ch - '0';
            else if ('A' <= ch && ch <= 'F') digit = ch - '7'; // hack 'A'-10 == '7'
            else if ('a' <= ch && ch <= 'f') digit = ch - 'W'; // hack 'a'-10 == 'W'
            else break; // invalid ch
            intPart = (intPart << 4) + digit; // intPart = intPart*16 + digit
        }
        if (end) *end = s; // write end of parsed value
        return intPart;
    }

    int _tostring(char* buffer, double f, int maxDecimals) noexcept
    {
        //// @todo Implement scientific notation?
        if (isnan(f)) {
            buffer[0] = 'n'; buffer[1] = 'a'; buffer[2] = 'n'; buffer[3] = '\0';
            return 3;
        }

        // we need ceil and floor for handling -DBL_MAX edge case, or -2.0119 edge case
        // if decimals=0 the float is rounded
        double i = maxDecimals == 0 ? round(f)
                 : f < -0.0 ? ceil(f) : floor(f);
        uint64 absIntegral = (int64)abs(i);

        char* end = buffer;
        if (f < -0.0) {
            *end++ = '-';
        }
        end += _tostring(end, absIntegral);

        if (maxDecimals > 0) {
            double g = abs(f - i); //  (-1.2) - (-1.0) --> -0.2
            if (g != 0.0) { // do we have a fraction ?
                // moving epsilon which has to be slightly smaller than maxDecimals
                double epsilon = 0.001;
                for (int d = 0; d < maxDecimals; ++d) {
                    epsilon *= 0.1;
                }
                *end++ = '.'; // place the fraction mark
                double x = g; // floats go way imprecise when *=10, perhaps should extract mantissa instead?
                for (int d = 0; d < maxDecimals; ++d) {
                    x *= 10;
                    int64 ix = (int64)x;
                    x -= ix;

                    // CUSTOM ROUNDING RULES:
                    // handle case of 0.199999 -> 1.99999 -> int(1), but should be int(2)
                    if (1.0 > x && x > 0.9999) {
                        int64 ix2 = (int64)round(x);
                        ix += ix2;
                        x -= ix2;
                    }

                    *end++ = '0' + (ix % 10);

                    if (x < epsilon) // break from 0.750000011 cases
                        break;
                    epsilon *= 10.0;
                }
            }
        }

        *end = '\0'; // null-terminate
        return (int)(end - buffer); // length of the string
    }

    template<class T> static int _tostring(char* buffer, char* end, T value) noexcept
    {
        char* start = end; // mark start for strrev after:
        if (value >= 0) {
            do { // write out remainder of 10 + '0' while we still have value
                *end++ = '0' + (value % 10);
                value /= 10;
            } while (value != 0);
        } else {
            do { // negative numbers require a special case because of INT64_MIN
                *end++ = '0' - (value % 10);
                value /= 10;
            } while (value != 0);
        }
        *end = '\0'; // always null-terminate

        char* rev = end; // for strrev, we'll need a helper pointer
        while (start < rev) { // reverse the string:
            char tmp = *start;
            *start++ = *--rev;
            *rev = tmp;
        }
        return (int)(end - buffer); // length of the string
    }

    template<class T> static char* _write_sign(char* buffer, const T& value) noexcept
    {
        char* end = buffer;
        if (value < 0) // if neg, abs and writeout '-'
            *end++ = '-'; // neg
        return end;
    }

    int _tostring(char* buffer, int value) noexcept
    {
        return _tostring(buffer, _write_sign(buffer, value), value);
    }
    int _tostring(char* buffer, int64 value) noexcept
    {
        return _tostring(buffer, _write_sign(buffer, value), value);
    }

    int _tostring(char* buffer, uint value) noexcept
    {
        return _tostring(buffer, buffer, value);
    }
    int _tostring(char* buffer, uint64 value) noexcept
    {
        return _tostring(buffer, buffer, value);
    }


    ///////////// to_string(wchar)

    std::string to_string(const wchar_t* str) noexcept
    {
    #if _WIN32
        int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
        std::string ret;
        ret.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, str, -1, (char*)ret.data(), len, nullptr, nullptr);
        return ret;
    #else
        std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
        return cvt.to_bytes(str);
    #endif
    }

    ///////////// line_parser

    bool line_parser::read_line(strview& out) noexcept
    {
        while (buffer.next(out, '\n')) {
            out.trim_end("\n\r", 2); // trim off any newlines
            return true;
        }
        return false; // no more lines
    }

    strview line_parser::read_line() noexcept
    {
        strview out;
        while (buffer.next(out, '\n')) {
            out.trim_end("\n\r", 2); // trim off any newlines
            return out;
        }
        return out;
    }


    ///////////// keyval_parser

    bool keyval_parser::read_line(strview& out) noexcept
    {
        strview line;
        while (buffer.next(line, '\n')) {
            // ignore comment lines or empty newlines
            if (line.str[0] == '#' || line.str[0] == '\n' || line.str[0] == '\r')
                continue; // skip to next line

            if (line.is_whitespace()) // is it all whitespace line?
                continue; // skip to next line

            // now we trim this beast and turn it into a c_str
            out = line.trim_start().split_first('#').trim_end();
            return true;
        }
        return false; // no more lines
    }
    bool keyval_parser::read_next(strview& key, strview& value) noexcept
    {
        strview line;
        while (read_line(line))
        {
            if (line.next(key, '=')) {
                key.trim();
                if (line.next(value, '=')) value.trim();
                else value.clear();
                return true;
            }
            return false;
        }
        return false;
    }


    ///////////// bracket_parser


    bracket_parser::bracket_parser(const void* data, int len) noexcept
        : buffer{(const char*)data, len}, depth{0}, line{1}
    {
        // Skips UTF-8 BOM if it exists
        if (buffer.starts_with("\xEF\xBB\xBF")) {
            buffer.skip(3);
        }
    }

    int bracket_parser::read_keyval(strview& key, strview& value) noexcept
    {
        for (; buffer.len; ++buffer.str, --buffer.len)
        {
            char ch = *buffer.str;
            if (ch == ' ' || ch == '\t' || ch == '\r')
                continue; // skip whitespace
            if (ch == '\n')
            {
                ++line;
                continue;
            }
            if (ch == '/' && buffer.str[1] == '/') 
            {
                buffer.skip_until('\n'); // skip comment lines
                ++line;
                continue;
            }
            if (ch == '{') // increase depth level on opened bracket
            {
                ++depth;
                continue;
            }
            if (ch == '}')
            {
                key = strview(buffer.str, 1);
                buffer.chomp_first();
                value.clear();
                return --depth; // } and depth dropped -- we're done parsing this section
            }
            
            // read current line:
            value = buffer.next_notrim("{}\r\n");
            value = value.split_first("//"); // C++ style comments need special handling
            value.next(key, " \t{}\r\n"); // read key until whitespace or other token finishers
            value.trim(" \t"); // give anything remaining to value
            return depth; // OK a value was read
        }
        return -1; // no more lines in Buffer
    }

    char bracket_parser::peek_next() const noexcept
    {
        for (strview buf = buffer; buf.len; ++buf.str, --buf.len)
        {
            char ch = *buf.str;
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
                continue; // skip whitespace
            if (ch == '#')
            {
                buf.skip_until('\n'); // skip comment lines
                continue;
            }
            return ch; // interesting char
        }
        return '\0'; // end of buffer
    }

    ////////////////////////////////////////////////////////////////////////////////
    
} // namespace rpp
