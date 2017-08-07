/**
 * String Tokenizer/View, Copyright (c) 2014 - Jorma Rebane
 */
#include "strview.h"
#include <cmath>
#include <cstdarg>

namespace rpp
{
    // This is same as memchr, but optimized for very small control strings
    bool strcontains(const char* str, int len, char ch) {
        for (int i = 0; i < len; ++i) if (str[i] == ch) return true; // found it.
        return false;
    }
    /**
     * @note Same as strpbrk, except we're not dealing with 0-term strings
     * @note This function is optimized for 4-8 char str and 3-4 char control.
     */
    const char* strcontains(const char* str, int nstr, const char* control, int ncontrol) {
        for (auto s = str; nstr; --nstr, ++s)
            if (strcontains(control, ncontrol, *s)) return s; // done
        return nullptr; // not found
    }
    NOINLINE bool strequals(const char* s1, const char* s2, int len) {
        for (int i = 0; i < len; ++i) 
            if (s1[i] != s2[i]) return false; // not equal.
        return true;
    }
    NOINLINE bool strequalsi(const char* s1, const char* s2, int len) {
        for (int i = 0; i < len; ++i) 
            if (::toupper(s1[i]) != ::toupper(s2[i])) return false; // not equal.
        return true;
    }


    ///////////// string view
    
    
    const char* strview::to_cstr(char* buf, int max) const
    {
        if (str[len] == '\0')
            return str;
        size_t n = size_t((len < max) ? len : max - 1);
        memcpy(buf, str, n);
        buf[n] = '\0';
        return buf;
    }

    const char* strview::to_cstr() const
    {
        if (str[len] == '\0')
            return str;
        static thread_local char buf[512];
        return to_cstr(buf, sizeof(buf));
    }

    bool strview::to_bool() const
    {
        const uint n = (uint)len;
        if (n > 4)
            return false; // can't be any of true literals
        return strequalsi(str, "true") ||
               strequalsi(str, "yes")  ||
               strequalsi(str, "on")   ||
               strequalsi(str, "1");
    }

    bool strview::is_whitespace() const
    {
        auto* s = str, *e = s+len;
        for (; s < e && *(byte*)s <= ' '; ++s) {} // loop while is whitespace
        return s == e;
    }
    
    int strview::compare(const char* s, int n) const
    {
        int len1 = len;
        int len2 = n;
        if (int res = memcmp(str, s, size_t(len1 < len2 ? len1 : len2)))
            return res;
        if (len1 < len2) return -1;
        if (len1 > len2) return +1;
        return 0;
    }
    
    int strview::compare(const char* s) const
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

    strview& strview::trim_start()
    {
        auto s = str;
        auto n = len;
        for (; n && *(byte*)s <= ' '; ++s, --n) {} // loop while is whitespace
        str = s; len = n; // result writeout
        return *this;
    }

    strview& strview::trim_start(char ch)
    {
        auto s = str;
        auto n = len;
        for (; n && *s == ch; ++s, --n) {}
        str = s; len = n; // result writeout
        return *this;
    }

    strview& strview::trim_start(const char* chars, int nchars)
    {
        auto s = str;
        auto n = len;
        for (; n && strcontains(chars, nchars, *s); ++s, --n) {}
        str = s; len = n;
        return *this;
    }

    strview& strview::trim_end()
    {
        auto n = len;
        auto e = str + n;
        for (; n && *--e <= ' '; --n) {} // reverse loop while is whitespace
        len = n;
        return *this;
    }

    strview& strview::trim_end(char ch)
    {
        auto n = len;
        auto e = str + n;
        for (; n && *--e == ch; --n) {}
        len = n;
        return *this;
    }

    strview& strview::trim_end(const char* chars, int nchars)
    {
        auto n = len;
        auto e = str + n;
        for (; n && strcontains(chars, nchars, *--e); --n) {}
        len = n;
        return *this;
    }

    const char* strview::find(const char* substr, int sublen) const
    {
        if (int n = sublen) //// @note lots of micro-optimization here
        {
            const char* needle = substr;
            const char* haystr = str;
            const char* hayend = haystr + len;
            int firstChar = *needle;
            while (haystr < hayend)
            {
                haystr = (const char*)memchr(haystr, firstChar, hayend - haystr);
                if (!haystr) 
                    return nullptr; // definitely not found

                if (memcmp(haystr, needle, n) == 0)
                    return haystr; // it's a match
                ++haystr; // no match, reset search from next char
            }
        }
        return nullptr;
    }

    const char* strview::rfind(char c) const
    {
        if (len <= 0) return nullptr;
        const char* p = str;
        const char* e = p + len - 1;
        for (; e >= p; --e) if (*e == c) return e;
        return nullptr;
    }

    const char* strview::findany(const char* chars, int n) const
    {
        const char* p = str;
        const char* e = p + len;
        for (; p < e; ++p) if (strcontains(chars, n, *p)) return p;
        return nullptr;
    }

    const char* strview::rfindany(const char* chars, int n) const
    {
        if (len <= 0) return nullptr;
        const char* p = str;
        const char* e = p + len - 1;
        for (; e >= p; --e) if (strcontains(chars, n, *e)) return e;
        return nullptr;
    }

    int strview::count(char ch) const
    {
        int count = 0;
        for (auto* p = str, *e = str+len; p < e; ++p)
            if (*p == ch) ++count;
        return count;
    }

    int strview::indexof(char ch) const
    {
        for (int i = 0; i < len; ++i)
            if (str[i] == ch) return i;
        return -1;
    }

    int strview::rindexof(char ch) const
    {
        for (int i = len-1; i >= 0; --i)
            if (str[i] == ch) return i;
        return -1;
    }

    int strview::indexofany(const char* chars, int n) const
    {
        for (int i = 0; i < len; ++i)
            if (strcontains(chars, n, str[i])) return i;
        return -1;
    }

    strview strview::split_first(char delim) const
    {
        if (auto splitEnd = (const char*)memchr(str, delim, size_t(len)))
            return strview(str, splitEnd); // if we find a separator, readjust end of strview to that
        return strview(str, len);
    }

    strview strview::split_first(const char* substr, int sublen) const
    {
        int l = len;
        const char* s = str;
        const char* e = s + l;
        char ch = *substr;
        while ((s = (const char*)memchr(s, ch, size_t(l))) != nullptr) {
            l = int(e - s);
            if (l >= sublen && strequals(s, substr, sublen)) {
                return strview(str, s);
            }
            --l; ++s;
        }
        return strview(str, len);
    }

    strview strview::split_second(char delim) const
    {
        if (auto splitstart = (const char*)memchr(str, delim, size_t(len)))
            return strview(splitstart + 1, str+len); // readjust start, also skip the char we split at
        return strview(str, len);
    }




    bool strview::next(strview& out, char delim)
    {
        bool result = next_notrim(out, delim);
        if (result && len) { ++str; --len; } // trim match
        return result;
    }

    bool strview::next(strview& out, const char* delims, int ndelims)
    {
        bool result = next_notrim(out, delims, ndelims);
        if (result && len) { ++str; --len; } // trim match
        return result;
    }





    bool strview::next_notrim(strview& out, char delim)
    {
        return _next_notrim(out, [delim](const char* s, int n) {
            return (const char*)memchr(s, delim, size_t(n));
        });
    }

    bool strview::next_notrim(strview& out, const char* delims, int ndelims)
    {
        return _next_notrim(out, [delims, ndelims](const char* s, int n) {
            return strcontains(s, n, delims, ndelims);
        });
    }


    strview strview::substr(int index, int length) const
    {
        int idx = index;
        if (idx > len)    idx = len;
        else if (idx < 0) idx = 0;
        
        int remaining = len - idx;
        if (remaining > length)
            remaining = length;

        return { str + idx, remaining };
    }

    strview strview::substr(int index) const
    {
        int idx = index;
        if (idx > len)    idx = len;
        else if (idx < 0) idx = 0;

        int remaining = len - idx;
        return { str + idx, remaining };
    }


    double strview::next_double()
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

    int strview::next_int()
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

    NOINLINE strview& strview::skip(int nchars)
    {
        auto s = str;
        auto l = len, n = nchars;
        for (; l && n > 0; --l, --n, ++s) {}
        str = s; len = l;
        return *this;
    }

    strview& strview::skip_until(char ch)
    {
        auto s = str;
        auto n = len;
        for (; n && *s != ch; --n, ++s) {}
        str = s; len = n;
        return *this;
    }
    strview& strview::skip_until(const char* substr, int sublen)
    {
        auto s = str, e = s + len;
        char ch = *substr; // starting char of the sequence
        while (s < e) {
            if (auto p = (const char*)memchr(s, ch, e - s)) {
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

    strview& strview::skip_after(char ch)
    {
        skip_until(ch);
        if (len) { ++str; --len; } // skip after
        return *this;
    }
    strview& strview::skip_after(const char* sstr, int slen)
    {
        skip_until(sstr, slen);
        if (len) { str += slen; len -= slen; } // skip after
        return *this;
    }


    static inline void foreach(char* str, int len, int func(int))
    {
        auto s = str, e = s + len;
        for (; s < e; ++s)
            *s = (char)func(*s);
    }
    strview& strview::to_lower()
    {
        foreach((char*)str, len, ::tolower); return *this;
    }
    strview& strview::to_upper()
    {
        foreach((char*)str, len, ::toupper); return *this;
    }
    char* strview::as_lower(char* dst) const {
        auto p = dst;
        auto s = str;
        for (int n = len; n > 0; --n)
            *p++ = (char)::tolower(*s++);
        *p = 0;
        return dst;
    }
    char* strview::as_upper(char* dst) const
    {
        auto p = dst;
        auto s = str;
        for (int n = len; n > 0; --n)
            *p++ = (char)::toupper(*s++);
        *p = 0;
        return dst;
    }
    string strview::as_lower() const
    {
        string ret;
        ret.reserve(size_t(len));
        auto s = (char*)str;
        for (int n = len; n > 0; --n)
            ret.push_back((char)::tolower(*s++));
        return ret;
    }
    string strview::as_upper() const
    {
        string ret;
        ret.reserve(size_t(len));
        auto s = (char*)str;
        for (int n = len; n > 0; --n)
            ret.push_back((char)::toupper(*s++));
        return ret;
    }




    char* to_lower(char* str, int len)
    {
        foreach(str, len, ::tolower); return str;
    }
    char* to_upper(char* str, int len)
    {
        foreach(str, len, ::toupper); return str;
    }
    string& to_lower(string& str)
    {
        foreach((char*)str.data(), (int)str.size(), ::tolower); return str;
    }
    string& to_upper(string& str)
    {
        foreach((char*)str.data(), (int)str.size(), ::toupper); return str;
    }

    char* replace(char* str, int len, char chOld, char chNew)
    {
        char* s = str, *e = s + len;
        for (; s < e; ++s) if (*s == chOld) *s = chNew;
        return str;
    }
    string& replace(string& str, char chOld, char chNew) {
        replace((char*)str.data(), (int)str.size(), chOld, chNew); return str;
    }


    strview& strview::replace(char chOld, char chNew)
    {
        auto s = (char*)str;
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
    double to_double(const char* str, int len, const char** end)
    {
        const char* s = str;
        const char* e = str + len;
        int64_t power   = 1;
        int64_t intPart = abs(to_int(s, len, &s));
        char ch         = *s;
        int exponent    = 0;

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
                intPart = (intPart << 3) + (intPart << 1) + (ch - '0'); // intPart = intPart*10 + digit
                power   = (power   << 3) + (power   << 1);              // power *= 10
            }
        }
        if (end) *end = s; // write end of parsed value
        double result = power == 1 ? double(intPart) : double(intPart) / double(power);
        if (exponent)
            result *= pow(10.0, (double)exponent);
        return result * sign;
    }

    // optimized for simplicity and performance
    int to_int(const char* str, int len, const char** end)
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
    // detects HEX integer strings as 0xBA or 0BA. Regular integers also parsed
    int to_inthx(const char* str, int len, const char** end)
    {
        const char* s = str;
        const char* e = str + len;
        unsigned intPart = 0;
        if (s[0] == '0' && s[1] == 'x') s += 2;
        for (char ch; s < e; ++s) {
            int digit;
            if ('0' <= (ch = *s) && ch <= '9') digit = ch - '0';
            else if ('A' <= ch && ch <= 'F')   digit = ch - '7'; // hack 'A'-10 == '7'
            else if ('a' <= ch && ch <= 'f')   digit = ch - 'W'; // hack 'a'-10 == 'W'
            else break; // invalid ch
            intPart = (intPart << 4) + digit; // intPart = intPart*16 + digit
        }
        if (end) *end = s; // write end of parsed value
        return intPart;
    }

    int _tostring(char* buffer, double f)
    {
        int64 value = (int64)f;
        f -= value; // -1.2 -= -1 --> -0.2
        if (value < 0) f = -f;
        char* end = buffer + _tostring(buffer, value);

        if (f != 0.0) { // do we have a fraction ?
            double cmp = 0.00001; // 6 decimal places max
            *end++ = '.'; // place the fraction mark
            double x = f; // floats go way imprecise when *=10, perhaps should extract mantissa instead?
            for (;;) {
                x *= 10;
                value = (int64)x;
                *end++ = '0' + (value % 10);
                x -= value;
                if (x < cmp) // break from 0.750000011 cases
                    break;
                cmp *= 10.0;
            }
        }
        *end = '\0'; // null-terminate
        return (int)(end - buffer); // length of the string
    }

    template<class T> static int _tostring(char* buffer, char* end, T value)
    {
        char* start = end; // mark start for strrev after:

        do { // write out remainder of 10 + '0' while we still have value
            *end++ = '0' + (value % 10);
            value /= 10;
        } while (value != 0);
        *end = '\0'; // always null-terminate

        char* rev = end; // for strrev, we'll need a helper pointer
        while (start < rev) { // reverse the string:
            char tmp = *start;
            *start++ = *--rev;
            *rev = tmp;
        }
        return (int)(end - buffer); // length of the string
    }

    template<class T> static char* _write_sign(char* buffer, T& value)
    {
        char* end = buffer;
        if (value < 0) { // if neg, abs and writeout '-'
            value = -value;	// flip sign
            *end++ = '-';	// neg
        }
        return end;
    }

    int _tostring(char* buffer, int value)
    {
        return _tostring(buffer, _write_sign(buffer, value), value);
    }
    int _tostring(char* buffer, int64 value)
    {
        return _tostring(buffer, _write_sign(buffer, value), value);
    }

    int _tostring(char* buffer, uint value)
    {
        return _tostring(buffer, buffer, value);
    }
    int _tostring(char* buffer, uint64 value)
    {
        return _tostring(buffer, buffer, value);
    }




    ///////////// line_parser

    bool line_parser::read_line(strview& out)
    {
        while (buffer.next(out, '\n')) {
            out.trim_end("\n\r", 2); // trim off any newlines
            return true;
        }
        return false; // no more lines
    }

    strview line_parser::read_line()
    {
        strview out;
        while (buffer.next(out, '\n')) {
            out.trim_end("\n\r", 2); // trim off any newlines
            return out;
        }
        return out;
    }


    ///////////// keyval_parser

    bool keyval_parser::read_line(strview& out)
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
    bool keyval_parser::read_next(strview& key, strview& value)
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


    bracket_parser::bracket_parser(const void* data, int len) 
        : buffer((const char*)data, len), depth(0), line(1)
    {
        // Skips UTF-8 BOM if it exists
        if (buffer.starts_with("\xEF\xBB\xBF")) {
            buffer.skip(3);
        }
    }

    int bracket_parser::read_keyval(strview& key, strview& value)
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

    char bracket_parser::peek_next() const
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


    string_buffer::~string_buffer() noexcept
    {
        if (cap > SIZE) free(ptr);
    }

    void string_buffer::clear()
    {
        ptr[len = 0] = '\0';
    }

    void string_buffer::reserve(int count) noexcept
    {
        int newlen = len + count + 1;
        if (newlen >= cap)
        {
            if (cap == SIZE)
            {
                // @todo Remove the loop
                while (cap < newlen) cap *= 2;
                ptr = (char*)memcpy(malloc(cap), buf, len);
            }
            else
            {
                while (cap < newlen) cap *= 2;
                ptr = (char*)realloc(ptr, cap);
            }
        }
    }
    
    void string_buffer::writef(const char* format, ...)
    {
        char buffer[4096];
        va_list ap; va_start(ap, format);
        int n = vsnprintf(buffer, sizeof(buffer), format, ap);
        if (n == -1)
            n = 4095;
        reserve(n);
        memcpy(&ptr[len], buffer, (size_t)n);
        len += n;
        ptr[len] = '\0';
    }

    void string_buffer::write(const strview& s)
    {
        reserve(s.len);
        char* dst = ptr + len;
        memcpy(dst, s.str, (size_t)s.len);
        len += s.len;
        ptr[len] = '\0';
    }

    void string_buffer::write(const char& value)
    {
        reserve(1);
        ptr[len++] = value;
        ptr[len] = '\0';
    }

    ////////////////////////////////////////////////////////////////////////////////

    int print(FILE* file, strview value) { return (int)fwrite(value.str, value.len, 1, file); }
    int print(FILE* file, char value)    { return (int)fwrite(&value, 1, 1, file);            }
    int print(FILE* file, byte value)    { char buf[4];  return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, short value)   { char buf[8];  return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, ushort value)  { char buf[8];  return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, int value)     { char buf[16]; return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, uint value)    { char buf[16]; return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, int64 value)   { char buf[32]; return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, uint64 value)  { char buf[32]; return (int)fwrite(buf, _tostring(buf, value), 1, file); }

    int print(strview value) { return print(stdout, value); }
    int print(char value)    { return print(stdout, value); }
    int print(byte value)    { return print(stdout, value); }
    int print(short value)   { return print(stdout, value); }
    int print(ushort value)  { return print(stdout, value); }
    int print(int value)     { return print(stdout, value); }
    int print(uint value)    { return print(stdout, value); }
    int print(int64 value)   { return print(stdout, value); }
    int print(uint64 value)  { return print(stdout, value); }

    int println(FILE* file) { return print(file, '\n');   }
    int println()           { return print(stdout, '\n'); }

    ////////////////////////////////////////////////////////////////////////////////
    
} // namespace rpp

namespace std
{
    ////////////////////////////////////////////////////////////////////////////////

    string to_string(bool trueOrFalse) noexcept
    {
        return trueOrFalse ? "true"s : "false"s;
    }

    string to_string(const char* cstr) noexcept
    {
        return cstr ? string{cstr} : string{};
    }

    ////////////////////////////////////////////////////////////////////////////////
}
