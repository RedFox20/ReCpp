#pragma once
/**
 * String Printing and Formatting, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#include <string>
#include "strview.h"
#include <cstdio>        // fprintf
#include <memory>        // std::shared_ptr
#include <unordered_map> // std::unordered_map for to_string extensions

#ifndef RPP_SPRINT_H
#define RPP_SPRINT_H 1
#endif

#ifndef RPPAPI
#  if _MSC_VER
#    define RPPAPI __declspec(dllexport)
#  else // clang/gcc
#    define RPPAPI __attribute__((visibility("default")))
#  endif
#endif

//// @note Some functions get inlined too aggressively, leading to some serious code bloat
////       Need to hint the compiler to take it easy ^_^'
#ifndef NOINLINE
#  ifdef _MSC_VER
#    define NOINLINE __declspec(noinline)
#  else
#    define NOINLINE __attribute__((noinline))
#  endif
#endif

//// @note Some strong hints that some functions are merely wrappers, so should be forced inline
#ifndef FINLINE
#  ifdef _MSC_VER
#    define FINLINE __forceinline
#  elif __APPLE__
#    define FINLINE inline __attribute__((always_inline))
#  else
#    define FINLINE __attribute__((always_inline))
#  endif
#endif

namespace rpp
{
    using std::string;

    ////////////////////////////////////////////////////////////////////////////////

    /**
     * Converts a single char 'x' into string "x"
     */
    RPPAPI string to_string(char v)   noexcept;

    /**
     * Simple and fast locale-agnostic to_string utilities:
     */
    RPPAPI string to_string(byte v)   noexcept;
    RPPAPI string to_string(short v)  noexcept;
    RPPAPI string to_string(ushort v) noexcept;
    RPPAPI string to_string(int v)    noexcept;
    RPPAPI string to_string(uint v)   noexcept;
    RPPAPI string to_string(long v)   noexcept;
    RPPAPI string to_string(ulong v)  noexcept;
    RPPAPI string to_string(int64 v)  noexcept;
    RPPAPI string to_string(uint64 v) noexcept;
    RPPAPI string to_string(float v)  noexcept;
    RPPAPI string to_string(double v) noexcept;

    /** @return "true" or "false" string */
    RPPAPI string to_string(bool trueOrFalse) noexcept;

    /**
     * @return std::string from cstring. If cstr == nullptr, returns empty string ""
     */
    RPPAPI string to_string(const char* cstr) noexcept;

    RPPAPI inline const string& to_string(const string& s) noexcept { return s; }

    template<class T> inline std::string to_string(const T* object)
    {
        using rpp::to_string;
        using namespace std::literals;
        return object ? to_string(*object) : "null"s;
    }

    /**
     * Always null terminated version of stringstream, which is compatible with strview
     * Not intended for moving or copying
     */
    struct RPPAPI string_buffer
    {
        static constexpr int SIZE = 512;
        char* ptr;
        int   len = 0;
        int   cap = SIZE;
        char  buf[SIZE];

        // controls the separator for generic template write calls
        // the default value " " will turn write("brown", "fox"); into "brown fox"
        // setting it to "..." will turn write("brown", "fox"); into "brown...fox"
        rpp::strview separator = " "_sv;

        FINLINE string_buffer() noexcept { ptr = buf; buf[0] = '\0'; }
        FINLINE explicit string_buffer(strview text) noexcept : string_buffer{} {
            write(text);
        }
        ~string_buffer() noexcept;

        string_buffer(string_buffer&&)                 = delete;
        string_buffer(const string_buffer&)            = delete;
        string_buffer& operator=(string_buffer&&)      = delete;
        string_buffer& operator=(const string_buffer&) = delete;

        FINLINE int size() const { return len; }
        FINLINE const char* c_str() const { return ptr; }
        FINLINE const char* data()  const { return ptr; }
        FINLINE rpp::strview view() const { return { ptr, len }; }
        FINLINE std::string  str()  const { return { ptr, ptr+len }; }
        FINLINE char back() const { return len ? ptr[len-1] : '\0'; }

        FINLINE explicit operator bool()         const { return len > 0; }
        FINLINE explicit operator rpp::strview() const { return { ptr, len }; }

        void clear();
        void reserve(int count) noexcept;
        void resize(int count) noexcept;

        /** @param count Size of buffer to emplace */
        char* emplace_buffer(int count) noexcept;
        void writef(const char* format, ...);
        void write(std::nullptr_t);
        void write(const string_buffer& sb);

        template<class T> FINLINE void write(const T& value)
        {
            write(to_string(value));
        }
        void write_ptr_begin();
        void write_ptr_end();
        template<class T> void write(const T* ptr)
        {
            if (ptr == nullptr) return write(nullptr);
            write_ptr_begin(); write(*ptr); write_ptr_end();
        }
        template<class T> FINLINE void write(T* ptr)                 { write((const T*)ptr); }
        template<class T> FINLINE void write(const std::weak_ptr<T>& p)   { write(p.lock()); }
        template<class T> FINLINE void write(const std::shared_ptr<T>& p) { write(p.get());  }

        template<int N> 
        FINLINE void write(const char (&value)[N])   { write(strview{ value }); }
        FINLINE void write(const std::string& value) { write(strview{ value }); }
        FINLINE void write(const char* value)        { write(strview{ value }); }

        void write(const strview& s);
        void write(char value);
        void write(bool   value);
        void write(rpp::byte bv);
        void write(short  value);
        void write(ushort value);
        void write(int    value);
        void write(uint   value);
        void write(long   value);
        void write(ulong  value);
        void write(int64  value);
        void write(uint64 value);
        void write(float  value);
        void write(double value);

        FINLINE string_buffer& operator<<(const std::string& value) { write(strview{ value }); return *this; }
        FINLINE string_buffer& operator<<(const char* value)        { write(strview{ value }); return *this; }
        FINLINE string_buffer& operator<<(char   value) { write(value); return *this; }
        FINLINE string_buffer& operator<<(bool   value) { write(value); return *this; }
        FINLINE string_buffer& operator<<(rpp::byte bv) { write(bv);    return *this; }
        FINLINE string_buffer& operator<<(short  value) { write(value); return *this; }
        FINLINE string_buffer& operator<<(ushort value) { write(value); return *this; }
        FINLINE string_buffer& operator<<(int    value) { write(value); return *this; }
        FINLINE string_buffer& operator<<(uint   value) { write(value); return *this; }
        FINLINE string_buffer& operator<<(long   value) { write(value); return *this; }
        FINLINE string_buffer& operator<<(ulong  value) { write(value); return *this; }
        FINLINE string_buffer& operator<<(int64  value) { write(value); return *this; }
        FINLINE string_buffer& operator<<(uint64 value) { write(value); return *this; }
        FINLINE string_buffer& operator<<(float  value) { write(value); return *this; }
        FINLINE string_buffer& operator<<(double value) { write(value); return *this; }
        FINLINE string_buffer& operator<<(const string_buffer& buf) { write(buf); return *this; }

        void writeln(); // \n
        void write_quote(); // <">
        void write_apos();  // <'>
        void write_colon(); // <: >
        void write_separator(); // this->separator

        /**
         * Stringifies and appends the input arguments one by one, filling gaps with delimiter
         * @see string_buffer::delimiter (default = ' ')
         * Ex: write("test:", 10, 20.1f);  --> "test: 10 20.1"
         */
        template<class T, class... Args> FINLINE void write(const T& first, const Args&... args)
        {
            write(first); write_separator(); write(args...);
        }
        template<class T> FINLINE void writeln(const T& value)
        {
            write(value); writeln();
        }
        template<class T, class... Args> FINLINE void writeln(const T& first, const Args&... args)
        {
            write(first, args...); writeln();
        }

        ////////////////////////////////////////////////////////////////////////////////////////////

        template<class T> FINLINE void prettyprint(const T& value) {
            write(to_string(value));
        }
        template<class T> void prettyprint(const T* ptr)
        {
            if (ptr == nullptr) return write(nullptr);
            write_ptr_begin(); prettyprint(*ptr); write_ptr_end();
        }
        template<class T> FINLINE void prettyprint(T* ptr)                      { prettyprint((const T*)ptr); }
        template<class T> FINLINE void prettyprint(const std::weak_ptr<T>& p)   { prettyprint(p.lock());  }
        template<class T> FINLINE void prettyprint(const std::shared_ptr<T>& p) { prettyprint(p.get());   }


        FINLINE void prettyprint(const string& value)  { write_quote(); write(value); write_quote(); }
        FINLINE void prettyprint(const strview& value) { write_quote(); write(value); write_quote(); }
        FINLINE void prettyprint(const char& value)    { write_apos();  write(value); write_apos();  }
        template<class K, class V> FINLINE void prettyprint(const K& key, const V& value) {
            prettyprint(key); write_colon(); prettyprint(value);
        }
        template<class K, class V> FINLINE void prettyprint(const std::pair<K, V>& pair) {
            prettyprint(pair.first); write_colon(); prettyprint(pair.second);
        }

        void pretty_cont_start(int count, bool newlines);
        void pretty_cont_item_start(bool newlines);
        void pretty_cont_item_end(int i, int count, bool newlines);
        void pretty_cont_end(int count);

        template<typename T, template<class, class...> class C, class... Args>
        NOINLINE void prettyprint(const C<T, Args...>& container, bool newlines = true)
        {
            int count = (int)container.size();
            pretty_cont_start(count, newlines);
            int i = 0;
            for (const T& item : container) {

                pretty_cont_item_start(newlines);
                prettyprint(item);
                pretty_cont_item_end(++i, count, newlines);
            }
            pretty_cont_end(count);
        }
        
        /**
         * Similar to write(...), but performs prettyprint formatting to each argument
         */
        template<class T, class... Args> FINLINE void prettyprint(const T& first, const Args&... args)
        {
            prettyprint(first); write_separator(); prettyprint(args...);
        }
        template<class T, class... Args> FINLINE void prettyprintln(const T& first, const Args&... args)
        {
            prettyprint(first, args...); writeln();
        }
    };
    
    ////////////////////////////////////////////////////////////////////////////////

    RPPAPI int print(FILE* file, strview value);
    RPPAPI int print(FILE* file, char value);
    RPPAPI int print(FILE* file, rpp::byte value);
    RPPAPI int print(FILE* file, short value);
    RPPAPI int print(FILE* file, ushort value);
    RPPAPI int print(FILE* file, int value);
    RPPAPI int print(FILE* file, uint value);
    RPPAPI int print(FILE* file, int64 value);
    RPPAPI int print(FILE* file, uint64 value);

    RPPAPI int print(strview value);
    RPPAPI int print(char value);
    RPPAPI int print(rpp::byte value);
    RPPAPI int print(short value);
    RPPAPI int print(ushort value);
    RPPAPI int print(int value);
    RPPAPI int print(uint value);
    RPPAPI int print(int64 value);
    RPPAPI int print(uint64 value);

    RPPAPI int println(FILE* file);
    RPPAPI int println();

    template<class T> int println(FILE* file, const T& value)
    {
        return print(value) + println(file);
    }
    template<class T> FINLINE int println(const T& value)
    {
        return println(stdout, value);
    }

    /**
     * Stringifies and appends the input arguments one by one, filling gaps with spaces, similar to Python print()
     * Ex: print("test:", 10, 20.1f);  --> "test: 10 20.1"
     */
    template<class T, class... Args> int print(FILE* file, const T& first, const Args&... args)
    {
        string_buffer buf;
        buf.write(first, args...);
        return (int)fwrite(buf.ptr, (size_t)buf.len, 1, file);
    }
    template<class T, class... Args> int print(const T& first, const Args&... args)
    {
        return print(stdout, first, args...);
    }

    /**
     * Stringifies and appends the input arguments one by one with an endline, filling gaps with spaces, similar to Python print()
     * Ex: print("test:", 10, 20.1f);  --> "test: 10 20.1\n"
     */
    template<class T, class... Args> int println(FILE* file, const T& first, const Args&... args)
    {
        string_buffer buf;
        buf.writeln(first, args...);
        return (int)fwrite(buf.ptr, (size_t)buf.len, 1, file);
    }
    template<class T, class... Args> int println(const T& first, const Args&... args)
    {
        return println(stdout, first, args...);
    }
    
    #if DEBUG
        #define debug_println(...) rpp::println(__VA_ARGS__)
    #else
        #define debug_println(...)
    #endif

    /**
     * Stringifies and appends the input arguments one by one, filling gaps with spaces, similar to Python print()
     * Ex: sprint("test:", 10, 20.1f);  --> "test: 10 20.1"
     */
    template<class T, class... Args> inline string sprint(const T& first, const Args&... args)
    {
        string_buffer buf;
        buf.write(first, args...);
        return buf.str();
    }
    template<class T, class... Args> inline string sprintln(const T& first, const Args&... args)
    {
        string_buffer buf;
        buf.writeln(first, args...);
        return buf.str();
    }

    inline const char* __format_wrap(const string& s)  { return s.c_str();    }
    inline const char* __format_wrap(const strview& s) { return s.to_cstr();  }
    template<class T> inline const T& __format_wrap(const T& t) { return t; }

    RPPAPI string __format(const char* format, ...); 

    template<class... Args> inline string format(const char* format, const Args&... args)
    {
        return __format(format, __format_wrap(args)...);
    }

    /////////////////////// useful container tostr extensions //////////////////////

    /** 
     *  @brief Outputs a linear container like vector<string>{"hello","world"} as:
     *  @code
     *  [2] = {
     *    "hello",
     *    "world"
     *  }
     *  @endcode
     *  
     *  Outputs a map like  unordered_map<strview,strview>{ {"key","value"}, {"name","john"} } as:
     *  @code
     *  [2] = {
     *    "key": "value",
     *    "name": "john"
     *  }
     *  @endcode
     *  
     *  With newLineSeparator = false the output is:
     *  @code
     *  [2] = { "hello", "world" }
     *  [2] = { "key": "value", "name": "john" }
     *  @endcode
     */
    template<class T, template<class,class...> class C, class... Args>
    NOINLINE string to_string(const C<T,Args...>& container, bool newLineSeparator = true) noexcept
    {
        rpp::string_buffer sb; sb.prettyprint(container, newLineSeparator); return sb.str();
    }
    template<class T> NOINLINE string to_string(const std::shared_ptr<T>& p) noexcept
    {
        rpp::string_buffer sb; sb.prettyprint(p); return sb.str();
    }
    template<class T> NOINLINE string to_string(const std::weak_ptr<T>& p) noexcept
    {
        rpp::string_buffer sb; sb.prettyprint(p); return sb.str();
    }

    ////////////////////////////////////////////////////////////////////////////////
}
