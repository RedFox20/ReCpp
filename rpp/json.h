#pragma once
#if false
/**
 * Basic JSON serialize/deserialize, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#if _MSC_VER
#  pragma warning(disable: 4251)
#endif
#include "file_io.h"
#include <unordered_map>
#include <vector>

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////////////////////////

    // variadic string type; can be a non-owning rpp::strview or an owning std::string
    // @todo Replace with std::variant<> when C++17 is available in XCode?
    class RPPAPI jstring
    {
        union {
            std::string str;
            strview     sv;
        };
        bool owns;
    public:
        jstring()                     noexcept : sv{},         owns{false} {}
        jstring(std::string&& s)      noexcept : str{move(s)}, owns{true}  {}
        jstring(const std::string& s) noexcept : str{s},       owns{true}  {}
        jstring(const strview& s)     noexcept : sv{s},        owns{false} {}
        ~jstring() noexcept;

        jstring(jstring&& fwd) noexcept;
        jstring(const jstring& copy);
        jstring& operator=(jstring&& fwd)       noexcept;
        jstring& operator=(const jstring& copy) noexcept;
        jstring& operator=(std::string&& s)      noexcept;
        jstring& operator=(const std::string& s) noexcept;
        jstring& operator=(const strview& s)     noexcept;

        operator std::string() const { return owns ? str : sv.to_string(); }
        operator strview()     const { return owns ? strview{str} : sv;    }
        template<class Str> jstring& assign(Str&& s) { return *this = std::forward<Str>(s); }

        const char* c_str()  const { return owns ? str.c_str()     : sv.str; }
        const char* data()   const { return owns ? str.c_str()     : sv.str; }
        int         size()   const { return owns ? (int)str.size() : sv.len; }
        int         length() const { return owns ? (int)str.size() : sv.len; }
        strview     view()   const { return owns ? strview{str}    : sv;     }
        void clear();

        bool operator==(const jstring& other) const { return view() == other.view(); }
        bool operator!=(const jstring& other) const { return view() != other.view(); }
        template<class Str> bool operator==(const Str& s) const { return view() == s; }
        template<class Str> bool operator!=(const Str& s) const { return view() != s; }

    };

    struct RPPAPI jstring_hash
    {
        size_t operator()(const jstring& s) const
        {
            return std::hash<rpp::strview>{}(s.view());
        }
    };

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

        using object_t = std::unordered_map<jstring, json, jstring_hash>;
        using array_t = std::vector<json>;

    private:
        type Type = null;
        union {
            object_t Object;
            array_t  Array;
            bool     Bool;
            double   Number;
            jstring  String;
        };

    public:
        json() noexcept;
        json(type defaultObjectType) noexcept;
        json(json&& fwd)       noexcept;
        json(const json& copy) noexcept;
        json& operator=(json&& fwd)       noexcept;
        json& operator=(const json& copy) noexcept;
        ~json() noexcept;

        template<class T> json(const jstring& key, const T& value) noexcept
            : Type(object), Object{ { key, json{value} } }
        {
        }
        
        template<class T> json(const tuple<jstring, T>& kv) noexcept
            : Type(object), Object{ kv }
        {
        }

        json(bool value)   noexcept : Type(boolean), Bool(value) {}
        json(double value) noexcept : Type(number), Number(value) {}
        json(int value)    noexcept : Type(number), Number(value) {}
        json(jstring&& value)      noexcept : Type(string), String(move(value)) {}
        json(const jstring& value) noexcept : Type(string), String(value)       {}

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
        jstring&  as_string ();
        const jstring& as_string()  const;

        // noexcept version of getting values; defaultValue must be specified in case of type mismatch
        bool     as_bool   (bool defaultValue)     const noexcept;
        double   as_number (double defaultValue)   const noexcept;
        int      as_integer(int defaultValue)      const noexcept;
        jstring  as_string (jstring defaultValue) const noexcept;

        // attempts to find a direct child object
        // throws if this type is not json::object
        bool     find_bool   (const strview& key, bool     defaultValue = false) const;
        double   find_number (const strview& key, double   defaultValue = 0.0)   const;
        int      find_integer(const strview& key, int      defaultValue = 0)     const;
        jstring  find_string (const strview& key, jstring  defaultValue = {})    const;
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
//        int line = 0;
//        int errline = 0;
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
            virtual ~stream() noexcept = default;
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
#endif
