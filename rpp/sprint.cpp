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

    void string_buffer::clear()
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
                ptr = (char*)memcpy(malloc(newcap), buf, len);
            else
                ptr = (char*)realloc(ptr, newcap);
            cap = newcap;
        }
    }

    char* string_buffer::emplace_buffer(int count) noexcept
    {
        reserve(count);
        char* result = &ptr[len];
        len += count;
        return result;
    }

    void string_buffer::writef(const char* format, ...)
    {
        char buffer[8192];
        va_list ap; va_start(ap, format);
        int n = vsnprintf(buffer, sizeof(buffer), format, ap);
        if (n < 0 || n >= (int)sizeof(buffer))
            n = sizeof(buffer) - 1;
        reserve(n);
        memcpy(&ptr[len], buffer, (size_t)n);
        len += n;
        ptr[len] = '\0';
    }


    void string_buffer::write(const string_buffer& sb) { write(sb.view()); }
    void string_buffer::write_ptr_begin() { write("*{");   }
    void string_buffer::write(std::nullptr_t)  { write("null"); }
    void string_buffer::write_ptr_end()   { write('}');    }

    void string_buffer::write(const strview& s)
    {
        reserve(s.len);
        char* dst = ptr + len;
        memcpy(dst, s.str, (size_t)s.len);
        len += s.len;
        ptr[len] = '\0';
    }

    void string_buffer::write(char value)
    {
        reserve(1);
        ptr[len++] = value;
        ptr[len] = '\0';
    }

    void string_buffer::write(bool value)   { write(value ? "true"_sv : "false"_sv); }
    void string_buffer::write(rpp::byte bv) { reserve(4);  len += _tostring(&ptr[len], bv);    }
    void string_buffer::write(short value)  { reserve(8);  len += _tostring(&ptr[len], value); }
    void string_buffer::write(ushort value) { reserve(8);  len += _tostring(&ptr[len], value); }
    void string_buffer::write(int value)    { reserve(16); len += _tostring(&ptr[len], value); }
    void string_buffer::write(uint value)   { reserve(16); len += _tostring(&ptr[len], value); }
    void string_buffer::write(int64 value)  { reserve(32); len += _tostring(&ptr[len], value); }
    void string_buffer::write(uint64 value) { reserve(32); len += _tostring(&ptr[len], value); }
    void string_buffer::write(float value)  { reserve(32); len += _tostring(&ptr[len], value); }
    void string_buffer::write(double value) { reserve(48); len += _tostring(&ptr[len], value); }

    void string_buffer::writeln()     { write('\n'); }
    void string_buffer::write_quote() { write('"');  }
    void string_buffer::write_apos()  { write('\''); }
    void string_buffer::write_colon() { write(": "); }
    void string_buffer::write_separator() { write(separator); }

    void string_buffer::pretty_cont_start(int count, bool newlines)
    {
        if (count == 0) return write("{}");
        if (count > 4) { write('['); write(count); write("] = { "); }
        else { write("{ "); }
        if (newlines) write('\n');
    }
    void string_buffer::pretty_cont_item_start(bool newlines)
    {
        if (newlines) write("  ");
    }
    void string_buffer::pretty_cont_item_end(int i, int count, bool newlines)
    {
        if (i < count) write(", ");
        if (newlines) write('\n');
    }
    void string_buffer::pretty_cont_end(int count)
    {
        if (count > 0) write(" }");
    }
    ////////////////////////////////////////////////////////////////////////////////

    int print(FILE* file, strview value) { return (int)fwrite(value.str, value.len, 1, file); }
    int print(FILE* file, char value)    { return (int)fwrite(&value, 1, 1, file); }
    int print(FILE* file, rpp::byte value){char buf[8];  return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, short value)   { char buf[8];  return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, ushort value)  { char buf[8];  return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, int value)     { char buf[16]; return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, uint value)    { char buf[16]; return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, int64 value)   { char buf[32]; return (int)fwrite(buf, _tostring(buf, value), 1, file); }
    int print(FILE* file, uint64 value)  { char buf[32]; return (int)fwrite(buf, _tostring(buf, value), 1, file); }

    int print(strview value)             { return print(stdout, value); }
    int print(char value)                { return print(stdout, value); }
    int print(rpp::byte value)           { return print(stdout, value); }
    int print(short value)               { return print(stdout, value); }
    int print(ushort value)              { return print(stdout, value); }
    int print(int value)                 { return print(stdout, value); }
    int print(uint value)                { return print(stdout, value); }
    int print(int64 value)               { return print(stdout, value); }
    int print(uint64 value)              { return print(stdout, value); }

    int println(FILE* file)              { return print(file, '\n'); }
    int println()                        { return print(stdout, '\n'); }

    string __format(const char* format, ...)
    {
        char buf[8192];
        va_list ap;
        va_start(ap, format);
        int n = vsnprintf(buf, sizeof(buf), format, ap);
        if (n < 0 || n >= (int)sizeof(buf)) {
            n = sizeof(buf)-1;
            buf[n] = '\0';
        }
        return string{buf, buf+n};
    }

    ////////////////////////////////////////////////////////////////////////////////

    string to_string(char v)   noexcept { return string(&v, 1ul); }
    string to_string(byte v)   noexcept { char buf[8];  return string(buf, buf + _tostring(buf, v)); }
    string to_string(short v)  noexcept { char buf[8];  return string(buf, buf + _tostring(buf, v)); }
    string to_string(ushort v) noexcept { char buf[8];  return string(buf, buf + _tostring(buf, v)); }
    string to_string(int v)    noexcept { char buf[16]; return string(buf, buf + _tostring(buf, v)); }
    string to_string(uint v)   noexcept { char buf[16]; return string(buf, buf + _tostring(buf, v)); }
    string to_string(long v)   noexcept { char buf[16]; return string(buf, buf + _tostring(buf, v)); }
    string to_string(ulong v)  noexcept { char buf[16]; return string(buf, buf + _tostring(buf, v)); }
    string to_string(int64 v)  noexcept { char buf[40]; return string(buf, buf + _tostring(buf, v)); }
    string to_string(uint64 v) noexcept { char buf[40]; return string(buf, buf + _tostring(buf, v)); }
    string to_string(float v)  noexcept { char buf[32]; return string(buf, buf + _tostring(buf, v)); }
    string to_string(double v) noexcept { char buf[32]; return string(buf, buf + _tostring(buf, v)); }

    string to_string(bool trueOrFalse) noexcept
    {
        using namespace std::literals;
        return trueOrFalse ? "true"s : "false"s;
    }

    string to_string(const char* cstr) noexcept
    {
        return cstr ? string{ cstr } : string{};
    }

    ////////////////////////////////////////////////////////////////////////////////
}
