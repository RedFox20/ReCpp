#pragma once
/**
 * Basic JSON serialize/deserialize, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#if _MSC_VER
#  pragma warning(disable: 4251)
#endif
#include "file_io.h"
#include <iostream>

#ifndef RPPAPI
#  if _MSC_VER
#    define RPPAPI __declspec(dllexport)
#  else // clang/gcc
#    define RPPAPI __attribute__((visibility("default")))
#  endif
#endif

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////////////////////////

    class RPPAPI json
    {
    public:
        enum type
        {
            null,
            object,
            array,
            boolean,
            number,
            string,
        };
        
        // variadic string type; can be a non-owning rpp::strview or an owning std::string
        // @todo Replace with std::variant<> when C++17 is available in XCode?
        class string_t
        {
            union {
                std::string str;
                strview     sv;
            };
            bool owns;
        public:
            string_t()                     noexcept : sv{},         owns{false} {}
            string_t(std::string&& s)      noexcept : str{move(s)}, owns{true}  {}
            string_t(const std::string& s) noexcept : str{s},       owns{true}  {}
            string_t(const strview& s)     noexcept : sv{s},        owns{false} {}
            ~string_t() noexcept;

            string_t(string_t&& fwd) noexcept;
            string_t(const string_t& copy);
            string_t& operator=(string_t&& fwd)       noexcept;
            string_t& operator=(const string_t& copy) noexcept;
            string_t& operator=(std::string&& s)      noexcept;
            string_t& operator=(const std::string& s) noexcept;
            string_t& operator=(const strview& s)     noexcept;

            operator std::string() const { return owns ? str : sv.to_string(); }
            operator strview()     const { return owns ? strview{str} : sv;    }
            template<class Str> string_t& assign(Str&& s) { return *this = forward<Str>(s); }

            const char* c_str()  const { return owns ? str.c_str()     : sv.str; }
            const char* data()   const { return owns ? str.c_str()     : sv.str; }
            int         size()   const { return owns ? (int)str.size() : sv.len; }
            int         length() const { return owns ? (int)str.size() : sv.len; }
            strview     view()   const { return owns ? strview{str}    : sv;     }
            void clear();

            bool operator==(const string_t& other) const { return view() == other.view(); }
            bool operator!=(const string_t& other) const { return view() != other.view(); }
            template<class Str> bool operator==(const Str& s) const { return view() == s; }
            template<class Str> bool operator!=(const Str& s) const { return view() != s; }
            struct hash {
                size_t operator()(const string_t& s) const {
                    return std::hash<rpp::strview>{}(s.view());
                }
            };
        };

        typedef unordered_map<string_t, json, string_t::hash> object_t;
        typedef vector<json>                  array_t;

    private:
        type Type = null;
        union {
            object_t Object;
            array_t  Array;
            bool     Bool;
            double   Number;
            string_t String;
        };

    public:
        json() noexcept : Type(null) {}
        json(type defaultObjectType) noexcept;
        json(json&& fwd)       noexcept;
        json(const json& copy) noexcept;
        json& operator=(json&& fwd)       noexcept;
        json& operator=(const json& copy) noexcept;
        ~json() noexcept;

        template<class T> json(const string_t& key, const T& value) noexcept
            : Type(object), Object{ { key, json{value} } }
        {
        }
        
        template<class T> json(const tuple<string_t, T>& kv) noexcept
            : Type(object), Object{ kv }
        {
        }

        json(bool value)   noexcept : Type(boolean), Bool(value) {}
        json(double value) noexcept : Type(number), Number(value) {}
        json(int value)    noexcept : Type(number), Number(value) {}
        json(string_t&& value)      noexcept : Type(string), String(move(value)) {}
        json(const string_t& value) noexcept : Type(string), String(value)       {}

        json& assign(json&& fwd)       noexcept { return *this = move(fwd); }
        json& assign(const json& copy) noexcept { return *this = copy;      }

        // reverts this json object to its default empty state, such as an empty map, array, etc.
        void clear() noexcept;

        // Reinitialize to a default object type
        void set(type defaultObjectType);

        /**
         * @return if json::object returns number of key-value pairs
         * @return if json::array returns number of array elements
         * @return if json::string returns number of bytes in the string (assume UTF-8)
         * @return In all other cases returns 0
         */
        int size() const noexcept;

        bool is_null()   const { return Type == null;   }
        bool is_object() const { return Type == object; }
        bool is_array()  const { return Type == array;  }
        bool is_bool()   const { return Type == boolean;}
        bool is_number() const { return Type == number; }
        bool is_string() const { return Type == string; }

        /**
         * @return String descriptor of the type, eg "null", "object", etc.
         */
        const char* type_string() const;


        json& operator[](int index);
        const json& operator[](int index) const;

        json& operator[](const strview& key);
        const json& operator[](const strview& key) const;

        json* find(const strview& key);
        const json* find(const strview& key) const;

        // gets the value, throws if type doesn't match
        bool      as_bool   () const;
        double    as_number () const;
        int       as_integer() const;
        string_t& as_string ();
        const string_t& as_string()  const;

        // noexcept version of getting values; defaultValue must be specified in case of type mismatch
        bool     as_bool   (bool defaultValue)     const noexcept;
        double   as_number (double defaultValue)   const noexcept;
        int      as_integer(int defaultValue)      const noexcept;
        string_t as_string (string_t defaultValue) const noexcept;

        // attempts to find a direct child object
        // throws if this type is not json::object
        bool     find_bool   (const strview& key, bool     defaultValue = false) const;
        double   find_number (const strview& key, double   defaultValue = 0.0)   const;
        int      find_integer(const strview& key, int      defaultValue = 0)     const;
        string_t find_string (const strview& key, string_t defaultValue = {})    const;
    };
    
    
    /**
     * Combines memory buffer with a root json object
     */
    class json_parser : public json
    {
    public:
        enum error_handling { nothrow, throw_on_error, };

    private:
        load_buffer buffer;
        std::string err;
        int line = 0;
        int errline = 0;
        error_handling errors = nothrow;
        bool stralloc = false;

    public:
        json_parser() noexcept : json{object} {}
        explicit json_parser(strview filePath) : json{object} {
            parse_file(filePath);
        }

        /**
         * if parse_failed(), gives an error string such as:
         * "[file]:[line] Expected closing brace '}' but found '[char]' instead."
         */
        strview error_string() const { return err; }
        const char* error()    const { return err.c_str(); }

        bool parse_success() const { return err.empty(); }
        bool parse_failed()  const { return !err.empty(); }

        bool set_error(strview error);
        bool set_error(const char* format, ...);

        /**
         * If set to true, json_parser will not store any string views
         * and will instead reallocate all string values.
         * @note This will make parsing significantly slower, but allows
         *       deleting the original input data.
         *       If this is not set, then the original data must remain allocated
         *       until json_parser object is destroyed
         */
        void realloc_strings(bool realloc = true) { stralloc = realloc; }

        /**
         * Parses a given UTF-8 byte buffer as json
         * @param buffer JSON data in UTF-8 format
         * @param errors [nothrow] if set to error_handling::throw_on_error, an exception is thrown on error.
         * @return true on success, false on error (check json_parser::error())
         */
        bool parse_data(strview buffer, error_handling errors = nothrow);

        /**
         * Parses an UTF-8 JSON file. Please don't bother with UCS-16. Just embrace UTF-8.
         * @warning This loads the entire file into memory, so it's not suitable for parsing
         *          incredibly huge json files... in which case you should probably NOT be using JSON.
         * @param filePath Local path to JSON file
         * @param errors [nothrow] if set to error_handling::throw_on_error, an exception is thrown on error.
         * @return true on success, false on error (check json_parser::error())
         */
        bool parse_file(strview filePath, error_handling errors = nothrow);

        /**
         * Parses an UTF-8 input stream. This uses a different stream parser that consumes input
         * as it arrives, token by token. Perfect for incredibly slow HDD-s(rare nowadays) or for network parsing.
         * @note Look at rpp::json benchmarks for parse_stream network stream vs download & parse_data
         * @param stream Input stream to read bytes from.
         * @param errors [nothrow] if set to error_handling::throw_on_error, an exception is thrown on error.
         * @return true on success, false on error (check json_parser::error())
         */
        bool parse_stream(std::istream& stream, error_handling errors = nothrow);

        // generic stream, in case you don't use std::istream
        struct stream
        {
            /**
             * Reads up to {max} bytes from this stream into buf.
             * @param buf Buffer to fill by reading bytes from the stream
             * @param max Maximum capacity of the buffer. Please don't overflow...
             * @return Number of bytes actually read. Return <= 0 for End Of Stream.
             */
            virtual int read(char* buf, int max) = 0;
            virtual ~stream() noexcept {}
        };

        /**
         * Parses an UTF-8 input stream. This uses a different stream parser that consumes input
         * as it arrives, token by token. Perfect for incredibly slow HDD-s(rare nowadays) or for network parsing.
         * @note You can implement your own json_parser::stream wrapper to hook into the parser from any kind of stream.
         * @param stream Input stream to read bytes from.
         * @param errors [nothrow] if set to error_handling::throw_on_error, an exception is thrown on error.
         * @return true on success, false on error (check json_parser::error())
         */
        bool parse_stream(stream& stream, error_handling errors = nothrow);
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////
}
