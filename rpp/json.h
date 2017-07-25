/**
 * Basic JSON serialize/deserialize, Copyright (c) 2017 - Jorma Rebane
 */
#ifndef RPP_JSON_H
#define RPP_JSON_H
#include "file_io.h"
#include "debugging.h"

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////////////////////////

    class json
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

        class string_t // variadic string type; can be a string view or an std::string
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

            string_t(string_t&& fwd);
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
        load_buffer buffer;
        
    public:

        json_parser() noexcept : json{object} {}
        json_parser(strview filePath) : json{object} {
            parse_file(filePath);
        }

        void parse_file(strview filePath)
        {

        }
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif /* RPP_JSON_H */
