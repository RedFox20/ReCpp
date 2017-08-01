#include "json.h"
#include "debugging.h"

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////////////////////////

    using string_t = json::string_t;

    json::json(json::type defaultObjectType) noexcept
    {
        set(defaultObjectType);
    }

    json::json(json&& fwd) noexcept
    {
        switch (Type = fwd.Type) {
            case null:    break;
            case object:  new (&Object) object_t{move(fwd.Object)}; break;
            case array:   new (&Array)  array_t {move(fwd.Array)};  break;
            case boolean: Bool   = fwd.Bool;   break;
            case number:  Number = fwd.Number; break;
            case string:  new (&String) string_t{move(fwd.String)}; break;
        }
        fwd.Type = null;
    }

    json::json(const json& copy) noexcept
    {
        switch (Type = copy.Type) {
            case null:    break;
            case object:  new (&Object) object_t{copy.Object}; break;
            case array:   new (&Array)  array_t {copy.Array};  break;
            case boolean: Bool   = copy.Bool;   break;
            case number:  Number = copy.Number; break;
            case string:  new (&String) string_t{copy.String}; break;
        }
    }

    json& json::operator=(json&& fwd) noexcept
    {
        if (Type == fwd.Type)
        {
            switch (Type) {
                case null:    break;
                case object:  swap(Object, fwd.Object); break;
                case array:   swap(Array,  fwd.Array);  break;
                case boolean: swap(Bool,   fwd.Bool);   break;
                case number:  swap(Number, fwd.Number); break;
                case string:  swap(String, fwd.String); break;
            }
        }
        else if (Type == null)
        {
            new (this) json{(json&&)fwd};
        }
        else
        {
            json temp { (json&&)fwd };    // json temp = move(fwd);
            new (&fwd) json{move(*this)}; // fwd  = move(this);
            new (this) json{move(temp)};  // this = move(temp);
        }
        return *this;
    }

    json& json::operator=(const json& copy) noexcept
    {
        if (this != &copy)
        {
            set(copy.Type);
            switch (Type) {
                case null:    break;
                case object:  Object = copy.Object; break;
                case array:   Array  = copy.Array;  break;
                case boolean: Bool   = copy.Bool;   break;
                case number:  Number = copy.Number; break;
                case string:  String = copy.String; break;
            }
        }
        return *this;
    }

    json::~json() noexcept
    {
        switch (Type) {
            case null:    break;
            case object:  Object.~object_t(); break;
            case array:   Array.~array_t();   break;
            case boolean: break;
            case number:  break;
            case string:  String.~string_t(); break;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////

    void json::clear() noexcept
    {
        switch (Type) {
            case null:    break;
            case object:  Object.clear(); break;
            case array:   Array.clear();  break;
            case boolean: Bool   = false; break;
            case number:  Number = 0.0;   break;
            case string:  String.clear(); break;
        }
    }

    void json::set(json::type defaultObjectType)
    {
        if (Type == defaultObjectType) {
            clear(); // objects are same, so just clear it
            return;
        }

        // destroy and reconstruct the object
        this->~json();
        switch (Type = defaultObjectType) {
            case null:    break;
            case object:  new (&Object) object_t{}; break;
            case array:   new (&Array)  array_t{};  break;
            case boolean: Bool   = false;           break;
            case number:  Number = 0.0;             break;
            case string:  new (&String) string_t{}; break;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////

    int json::size() const noexcept
    {
        switch (Type) {
            default:
            case null:    return 0;
            case object:  return (int)Object.size();
            case array:   return (int)Array.size();
            case boolean: return 0;
            case number:  return 0;
            case string:  return (int)String.size();
        }
    }

    const char *json::type_string() const
    {
        switch (Type) {
            default:      return "<?garbage?>";
            case null:    return "null";
            case object:  return "object";
            case array:   return "array";
            case boolean: return "boolean";
            case number:  return "number";
            case string:  return "string";
        }
    }

#define RPP_JSON_CHECK_IS_ARRAY(index) do { if (!is_array()) \
    ThrowErr("this[%d] expects json::array but this is json::%s", index, type_string()); } while(0)

#define RPP_JSON_CHECK_IS_OBJECT(key) do { if (!is_object()) \
    ThrowErr("this['%s'] expects json::object but this is json::%s", key.to_cstr(), type_string()); } while(0)

#define RPP_JSON_CHECK_IS_TYPE(what, type) do { if (!is_##type()) \
    ThrowErr(what "expects json::" #type " but this is json::%s", type_string()); } while(0)


    json& json::operator[](int index)
    {
        RPP_JSON_CHECK_IS_ARRAY(index);
        return Array[index];
    }
    const json& json::operator[](int index) const
    {
        RPP_JSON_CHECK_IS_ARRAY(index);
        return Array[index];
    }

    json& json::operator[](const strview& key)
    {
        RPP_JSON_CHECK_IS_OBJECT(key);
        return Object[key];
    }
    const json& json::operator[](const strview& key) const
    {
        if (auto* item = find(key))
            return *item;
        ThrowErr("json['%s'] key not found", key.to_cstr());
    }

    json* json::find(const strview& key)
    {
        RPP_JSON_CHECK_IS_OBJECT(key);
        auto it = Object.find(key);
        return it != Object.end() ? &it->second : nullptr;
    }
    const json* json::find(const strview& key) const
    {
        RPP_JSON_CHECK_IS_OBJECT(key);
        auto it = Object.find(key);
        return it != Object.end() ? &it->second : nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////

    bool json::as_bool() const
    {
        RPP_JSON_CHECK_IS_TYPE("as_bool()", bool);
        return Bool;
    }
    double json::as_number() const
    {
        RPP_JSON_CHECK_IS_TYPE("as_number()", number);
        return Number;
    }
    int json::as_integer() const
    {
        RPP_JSON_CHECK_IS_TYPE("as_integer()", number);
        return (int)Number;
    }
    string_t& json::as_string()
    {
        RPP_JSON_CHECK_IS_TYPE("as_string()", string);
        return String;
    }
    const string_t& json::as_string() const
    {
        RPP_JSON_CHECK_IS_TYPE("as_string()", string);
        return String;
    }

    bool json::as_bool(bool defaultValue) const noexcept
    {
        return is_bool() ? Bool : defaultValue;
    }
    double json::as_number(double defaultValue) const noexcept
    {
        return is_number() ? Number : defaultValue;
    }
    int json::as_integer(int defaultValue) const noexcept
    {
        return is_number() ? (int)Number : defaultValue;
    }
    string_t json::as_string(string_t defaultValue) const noexcept
    {
        return is_string() ? String : defaultValue;
    }

    bool json::find_bool(const strview &key, bool defaultValue) const
    {
        auto* item = find(key);
        return item ? item->as_bool(defaultValue) : defaultValue;
    }
    double json::find_number(const strview &key, double defaultValue) const
    {
        auto* item = find(key);
        return item ? item->as_number(defaultValue) : defaultValue;
    }
    int json::find_integer(const strview &key, int defaultValue) const
    {
        auto* item = find(key);
        return item ? item->as_integer(defaultValue) : defaultValue;
    }
    string_t json::find_string(const strview &key, string_t defaultValue) const
    {
        auto* item = find(key);
        return item ? item->as_string(defaultValue) : defaultValue;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////

    string_t::string_t(string_t&& fwd) : owns(fwd.owns)
    {
        if (owns) str = move(fwd.str), fwd.owns = false;
        else sv = fwd.sv;
    }

    string_t::string_t(const string_t& copy) : owns(copy.owns) {
        if (owns) str = copy.str; else sv = copy.sv;
    }

    string_t& string_t::operator=(string_t&& fwd) noexcept
    {
        string_t temp { move(fwd) };
        new (&fwd) string_t{move(*this)};
        new (this) string_t{move(temp)};
        return *this;
    }

    string_t& string_t::operator=(const string_t& copy) noexcept
    {
        if (this == &copy) return *this;
        if (copy.owns)     return *this = copy.str;
        else               return *this = copy.sv;
    }

    string_t& string_t::operator=(std::string&& s) noexcept
    {
        if (owns) str = move(s);
        else new (&str) std::string{move(s)}, owns = true;
        return *this;
    }

    string_t& string_t::operator=(const std::string& s) noexcept
    {
        if (owns) str = s;
        else new (&str) std::string{move(s)}, owns = true;
        return *this;
    }

    template<class T> static inline void destruct(T& obj) { obj.~T(); }

    string_t& string_t::operator=(const strview& s) noexcept
    {
        if (owns) destruct(str), owns = false;
        sv = s;
        return *this;
    }

    string_t::~string_t() noexcept
    {
        if (owns)
            destruct(str);
    }

    void string_t::clear()
    {
        if (owns) destruct(str), owns = false;
        else sv.clear();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////

}
