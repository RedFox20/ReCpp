#include "sprint.h"
#include <cstdarg> // va_start
#include <cstdlib> // malloc/free/realloc

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////////

    string_buffer::~string_buffer() noexcept
    {
        if (cap > SIZE) free(ptr);
    }

    string_buffer::string_buffer(string_buffer&& sb) noexcept
    {
        len = sb.len;
        buf[0] = '\0';
        if (sb.ptr == sb.buf)
        {
            ptr = buf;
            memcpy(buf, sb.buf, len);
        }
        else
        {
            ptr = sb.ptr;
            cap = sb.cap;
        }
        sb.ptr = nullptr;
        sb.len = 0;
        sb.cap = 0;
    }
    
    string_buffer& string_buffer::operator=(string_buffer&& sb) noexcept
    {
        string_buffer tmp { std::move(*this) };
        new (this) string_buffer{ std::move(sb)  }; // NOLINT
        new (&sb)  string_buffer{ std::move(tmp) }; // NOLINT
        return *this;
    }

    void string_buffer::clear() noexcept
    {
        ptr[len = 0] = '\0';
    }

    void string_buffer::reserve(int count) noexcept
    {
        int newlen = len + count + 1;
        if (newlen > cap)
        {
            int align = cap; // cap is always aligned
            int newcap = newlen + align;
            if (int rem = newcap % align)
                newcap += align - rem;

            if (cap == SIZE)
            {
                if (char* newp = (char*)malloc(newcap))
                {
                    ptr = (char*)memcpy(newp, buf, len);
                    cap = newcap;
                }
            }
            else
            {
                if (char* newp = (char*)realloc(ptr, newcap))
                {
                    ptr = newp;
                    cap = newcap;
                }
            }
        }
    }

    void string_buffer::resize(int count) noexcept
    {
        reserve(count);
        len = count;
    }

    
    void string_buffer::append(const char* str, int slen) noexcept
    {
        char* dst = emplace_buffer(slen);
        memcpy(dst, str, (size_t)slen);
        ptr[len] = '\0';
    }

    char* string_buffer::emplace_buffer(int count) noexcept
    {
        reserve(count);
        char* result = &ptr[len];
        len += count;
        return result;
    }

    void string_buffer::writef(PRINTF_FMTSTR const char* format, ...) noexcept
    {
        va_list ap;
        va_start(ap, format);
        char buffer[8192];
        int n = vsnprintf(buffer, sizeof(buffer), format, ap);
        if (n < 0 || n >= (int)sizeof(buffer))
            n = sizeof(buffer) - 1;
        reserve(n);
        memcpy(&ptr[len], buffer, size_t(n));
        len += n;
        ptr[len] = '\0';
    }


    void string_buffer::write(const string_buffer& sb) noexcept { write(sb.view()); }
    void string_buffer::write_ptr_begin() noexcept     { write("*{");   }
    void string_buffer::write(std::nullptr_t) noexcept { write("null"); }
    void string_buffer::write_ptr_end() noexcept       { write('}');    }

    void string_buffer::write(strview s) noexcept
    {
        if (s.len <= 0) return;
        reserve(s.len);
        memcpy(&ptr[len], s.str, size_t(s.len));
        len += s.len;
        ptr[len] = '\0';
    }

    void string_buffer::write(char value) noexcept
    {
        reserve(1);
        ptr[len++] = value;
        ptr[len] = '\0';
    }

    void string_buffer::write(bool value)   noexcept { write(value ? "true"_sv : "false"_sv); }
    void string_buffer::write(rpp::byte bv) noexcept { reserve(4);  len += _tostring(&ptr[len], bv);    }
    void string_buffer::write(short value)  noexcept { reserve(8);  len += _tostring(&ptr[len], value); }
    void string_buffer::write(ushort value) noexcept { reserve(8);  len += _tostring(&ptr[len], value); }
    void string_buffer::write(int value)    noexcept { reserve(16); len += _tostring(&ptr[len], value); }
    void string_buffer::write(uint value)   noexcept { reserve(16); len += _tostring(&ptr[len], value); }
    void string_buffer::write(long value)   noexcept { reserve(16); len += _tostring(&ptr[len], value); }
    void string_buffer::write(ulong value)  noexcept { reserve(16); len += _tostring(&ptr[len], value); }
    void string_buffer::write(int64 value)  noexcept { reserve(32); len += _tostring(&ptr[len], value); }
    void string_buffer::write(uint64 value) noexcept { reserve(32); len += _tostring(&ptr[len], value); }
    void string_buffer::write(float value)  noexcept { reserve(32); len += _tostring(&ptr[len], value); }
    void string_buffer::write(double value) noexcept { reserve(48); len += _tostring(&ptr[len], value); }

#if RPP_ENABLE_UNICODE
    void string_buffer::write_utf16_as_utf8(const char16_t* utf16, int utflength) noexcept
    {
        // if UTF-16 contains only ASCII, then UTF-8 will be the same length
        // so best case scenario we can reserve the same length, however we will dynamically append
        reserve(utflength);

        for (int i = 0; i < utflength; ++i)
        {
            char32_t codepoint = utf16[i];
            if (codepoint >= 0xD800 && codepoint <= 0xDBFF && (i+1) < utflength)
            {
                // Handle surrogate pairs
                char32_t high = codepoint - 0xD800;
                char32_t low = utf16[++i] - 0xDC00; // consume i+1 pair
                codepoint = (high << 10) + low + 0x10000;
            }

            // Convert to UTF-8
            int char_size;
            if      (codepoint <= 0x7F)   char_size = 1;
            else if (codepoint <= 0x7FF)  char_size = 2;
            else if (codepoint <= 0xFFFF) char_size = 3;
            else                          char_size = 4;
            reserve(char_size);

            if (char_size == 1)
            {
                ptr[len++] = (static_cast<char>(codepoint));
            }
            else if (char_size == 2)
            {
                ptr[len++] = (static_cast<char>(0xC0 | (codepoint >> 6)));
                ptr[len++] = (static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            else if (char_size == 3)
            {
                ptr[len++] = (static_cast<char>(0xE0 | (codepoint >> 12)));
                ptr[len++] = (static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                ptr[len++] = (static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            else
            {
                ptr[len++] = (static_cast<char>(0xF0 | (codepoint >> 18)));
                ptr[len++] = (static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                ptr[len++] = (static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                ptr[len++] = (static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
        }
        ptr[len] = '\0';
    }
#endif // RPP_ENABLE_UNICODE

    static const char HEX[16]   = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };
    static const char HEXUP[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };

    void string_buffer::write_hex(const void* data, int numBytes, format_opt opt) noexcept
    {
        const char* hex = opt == uppercase ? HEXUP : HEX;
        const uint8_t* src = reinterpret_cast<const uint8_t*>(data);
        reserve(numBytes*2);
        for (int i = 0; i < numBytes; ++i)
        {
            uint8_t ch = src[i];
            ptr[len++] = hex[ch >> 4];
            ptr[len++] = hex[ch & 0x0f];
        }
        ptr[len] = '\0';
    }

    void string_buffer::write_ptr(const void* p, format_opt opt) noexcept
    {
        static_assert(sizeof(p) == 8 || sizeof(p) == 4, "sizeof(ptr) expected as 8 or 4");
        reserve(sizeof(p)*2 + 2);
        ptr[len++] = '0';
        ptr[len++] = 'x';
        const char* hex = opt == uppercase ? HEXUP : HEX;

        uint64_t v = (uint64_t)p;
        for (int i = int(sizeof(p)*2) - 1; i >= 0; --i) {
            ptr[len++] = hex[(v >> i*4) & 0x0f];
        }

        ptr[len] = '\0';
    }

    void string_buffer::writeln() noexcept     { write('\n'); }
    void string_buffer::write_quote() noexcept { write('"');  }
    void string_buffer::write_apos() noexcept  { write('\''); }
    void string_buffer::write_colon() noexcept { write(": "); }
    void string_buffer::write_separator() noexcept { write(separator); }

    void string_buffer::pretty_cont_start(int count, bool newlines) noexcept
    {
        if (count == 0) return write("{}");
        if (count > 4) { write('['); write(count); write("] = { "); }
        else { write("{ "); }
        if (newlines) write('\n');
    }
    void string_buffer::pretty_cont_item_start(bool newlines) noexcept
    {
        if (newlines) write("  ");
    }
    void string_buffer::pretty_cont_item_end(int i, int count, bool newlines) noexcept
    {
        if (i < count) write(", ");
        if (newlines) write('\n');
    }
    void string_buffer::pretty_cont_end(int count) noexcept
    {
        if (count > 0) write(" }");
    }
    ////////////////////////////////////////////////////////////////////////////////

    int print(FILE* file, strview value) noexcept { return (int)fwrite(value.str, value.len, 1, file); }
    int print(FILE* file, char value)    noexcept { return (int)fwrite(&value, 1, 1, file); }
    int print(FILE* file, rpp::byte value) noexcept{char buf[8];  return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, short value)   noexcept { char buf[8];  return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, ushort value)  noexcept { char buf[8];  return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, int value)     noexcept { char buf[16]; return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, uint value)    noexcept { char buf[16]; return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, int64 value)   noexcept { char buf[32]; return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, uint64 value)  noexcept { char buf[32]; return (int)fwrite(buf, _tostring(buf, value), 1, file); }

    int print(strview value)             noexcept { return print(stdout, value); }
    int print(char value)                noexcept { return print(stdout, value); }
    int print(rpp::byte value)           noexcept { return print(stdout, value); }
    int print(short value)               noexcept { return print(stdout, value); }
    int print(ushort value)              noexcept { return print(stdout, value); }
    int print(int value)                 noexcept { return print(stdout, value); }
    int print(uint value)                noexcept { return print(stdout, value); }
    int print(int64 value)               noexcept { return print(stdout, value); }
    int print(uint64 value)              noexcept { return print(stdout, value); }

    int println(FILE* file)              noexcept { return print(file, '\n'); }
    int println()                        noexcept { return print(stdout, '\n'); }

    std::string __format(PRINTF_FMTSTR const char* format, ...) noexcept
    {
        va_list ap;
        va_start(ap, format);
        char buf[8192];
        int n = vsnprintf(buf, sizeof(buf), format, ap);
        va_end(ap);
        if (n < 0 || n >= (int)sizeof(buf)) {
            n = sizeof(buf)-1;
            buf[n] = '\0';
        }
        return std::string{buf, buf+n};
    }

    ////////////////////////////////////////////////////////////////////////////////

    std::string to_string(char v)   noexcept { return std::string{&v, 1ul}; }
    std::string to_string(byte v)   noexcept { char buf[8];  return std::string{buf, buf + _tostring(buf, v)}; }
    std::string to_string(short v)  noexcept { char buf[8];  return std::string{buf, buf + _tostring(buf, v)}; }
    std::string to_string(ushort v) noexcept { char buf[8];  return std::string{buf, buf + _tostring(buf, v)}; }
    std::string to_string(int v)    noexcept { char buf[16]; return std::string{buf, buf + _tostring(buf, v)}; }
    std::string to_string(uint v)   noexcept { char buf[16]; return std::string{buf, buf + _tostring(buf, v)}; }
    std::string to_string(long v)   noexcept { char buf[16]; return std::string{buf, buf + _tostring(buf, v)}; }
    std::string to_string(ulong v)  noexcept { char buf[16]; return std::string{buf, buf + _tostring(buf, v)}; }
    std::string to_string(int64 v)  noexcept { char buf[40]; return std::string{buf, buf + _tostring(buf, v)}; }
    std::string to_string(uint64 v) noexcept { char buf[40]; return std::string{buf, buf + _tostring(buf, v)}; }
    std::string to_string(float v)  noexcept { char buf[32]; return std::string{buf, buf + _tostring(buf, v)}; }
    std::string to_string(double v) noexcept { char buf[32]; return std::string{buf, buf + _tostring(buf, v)}; }

    std::string to_string(bool trueOrFalse) noexcept
    {
        using namespace std::literals;
        return trueOrFalse ? "true"s : "false"s;
    }

    std::string to_string(const char* cstr) noexcept
    {
        return cstr ? std::string{ cstr } : std::string{};
    }

    ////////////////////////////////////////////////////////////////////////////////
}
