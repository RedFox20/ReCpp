#pragma once
#ifndef RPP_BINARY_SERIALIZER_H
#define RPP_BINARY_SERIALIZER_H
#include "binary_stream.h"

//// @note Some functions get inlined too aggressively, leading to some serious code bloat
////       Need to hint the compiler to take it easy ^_^'
#ifndef NOINLINE
    #ifdef _MSC_VER
    #  define NOINLINE __declspec(noinline)
    #else
    #  define NOINLINE __attribute__((noinline))
    #endif
#endif
//// @note Some strong hints that some functions are merely wrappers, so should be forced inline
#ifndef FINLINE
    #ifdef _MSC_VER
        #define FINLINE __forceinline
    #else
        #define FINLINE  __attribute__((always_inline))
    #endif
#endif

namespace rpp
{
    //////////////////////////////////////////////////////////////////////////////////

    template<int SIZE> constexpr uint _layoutv(const char (&s)[SIZE], uint i, char v) {
        return i < SIZE - 1 ? (s[i] == v ? i : _layoutv(s, i + 1, v)) : 0ull;
    }
    template<int SIZE> constexpr uint _layout(const char (&s)[SIZE], uint i = 0) {
        return i < SIZE - 1 ? (_layout(s, i + 1) * 10ull + _layoutv("?bwdqs", 0, s[i])) : 0ull;
    }
    // Provide an autoserialize string that describes the data layout
    // "bwdqs" 
    // 1 b - byte   (8-bit  - bool, char, uint8)
    // 2 w - word   (16-bit - short, uint16)
    // 3 d - dword  (32-bit - int, uint32)
    // 4 q - qword  (64-bit - int64, uint64)
    // 5 s - string (std::string)
    #define BINARY_SERIALIZE(Class, layoutString) \
        static const rpp::uint Layout = rpp::_layout(layoutString); \
        inline Class(rpp::socket_reader& r){layout=Layout; this->read(r);}

    struct binary_serializer
    {
        uint layout;
        // numbytes during read/write, initialized during read, uninitialized by default
        // used to skip messages and to validate the message
        int length;

        binary_serializer() {}
        binary_serializer(uint layout) : layout(layout), length(0) {}

        NOINLINE binary_stream& write(binary_stream& w) const noexcept;
        NOINLINE binary_stream& read(binary_stream& r) noexcept;

        /** @brief Returns the serialization layout size */
        NOINLINE int layout_size() const noexcept;
    };

    inline binary_stream& operator<<(binary_stream& w, const binary_serializer& s) 
    {
        return s.write(w); 
    }
    inline binary_stream& operator>>(binary_stream& r, binary_serializer& s) 
    {
        return s.read(r); 
    }

    //////////////////////////////////////////////////////////////////////////////////

    struct string_serializer : string_buffer
    {
        using string_buffer::string_buffer;
    };

    template<class T> struct member_serialize
    {
        using bserialize_func   = void(*)(const T* inst, int offset, binary_stream& w);
        using bdeserialize_func = void(*)(T* inst,       int offset, binary_stream& r);
        using sserialize_func   = void(*)(const T* inst, int offset, string_serializer& w);
        using sdeserialize_func = void(*)(T* inst,       int offset, strview token);
        int offset;
        strview name; // for named serialization such as json
        bserialize_func   bserialize;
        bdeserialize_func bdeserialize;
        sserialize_func   sserialize;
        sdeserialize_func sdeserialize;
    };

    template<class T> struct serializable
    {
        static vector<member_serialize<T>> members;
        serializable()
        {
            static bool introspection_complete = [this](){
                static_cast<T*>(this)->introspect();
                return true;
            }();
            (void)introspection_complete;
        }

        template<class U> static void binary_serialize(const T* inst, int offset, binary_stream& w)
        {
            U& var = *(U*)((byte*)inst + offset);
            w << var;
        }
        template<class U> static void binary_deserialize(T* inst, int offset, binary_stream& r)
        {
            U& var = *(U*)((byte*)inst + offset);
            r >> var;
        }

        template<class U> static void string_serialize(const T* inst, int offset, string_serializer& w)
        {
            U& var = *(U*)((byte*)inst + offset);
            w.write(var);
        }
        template<class U> static void string_deserialize(T* inst, int offset, strview token)
        {
            U& var = *(U*)((byte*)inst + offset);
            token >> var;
        }

        template<class U> void bind(U& elem) noexcept
        {
            member_serialize<T> m;
            m.offset       = int((char*)&elem - (char*)this);
            m.bserialize   = &serializable::binary_serialize<U>;
            m.bdeserialize = &serializable::binary_deserialize<U>;
            m.sserialize   = &serializable::string_serialize<U>;
            m.sdeserialize = &serializable::string_deserialize<U>;
            members.emplace_back(move(m));
        }

        template<class U> void bind_name(strview name, U& elem) noexcept
        {
            bind(elem);
            members.back().name = name;
        }

        template<class U, class... Args> void bind(U& first, Args&... args)
        {
            bind(first);
            bind(args...);
        }

        void serialize(binary_stream& w) const
        {
            const T* inst = static_cast<const T*>(this);
            for (member_serialize<T>& memberInfo : members)
            {
                memberInfo.bserialize(inst, memberInfo.offset, w);
            }
        }
        void deserialize(binary_stream& r)
        {
            T* inst = static_cast<T*>(this);
            for (member_serialize<T>& memberInfo : members)
            {
                memberInfo.bdeserialize(inst, memberInfo.offset, r);
            }
        }

        // serializes this into a single line "name1;value1;name2;value2;\n"
        void serialize(string_serializer& w) const
        {
            const T* inst = static_cast<const T*>(this);
            for (member_serialize<T>& memberInfo : members)
            {
                w.write(memberInfo.name);
                w.write(';');
                memberInfo.sserialize(inst, memberInfo.offset, w);
                w.write(';');
            }
            w.writeln();
        }
        // parses a single line from this string view and decomposes:
        // "name1;value1;name2;value2;\n"
        void deserialize(strview& r)
        {
            if (strview line = r.next('\n'))
            {
                T* inst = static_cast<T*>(this);
                for (member_serialize<T>& memberInfo : members)
                {
                    strview key   = line.next(';');
                    strview value = line.next(';');
                    if (key != memberInfo.name) {
                        throw std::runtime_error("string deserialize key does not match member name");
                    }
                    memberInfo.sdeserialize(inst, memberInfo.offset, value);
                }
            }
        }
    };

    template<class T> vector<member_serialize<T>> serializable<T>::members;

    template<class T> binary_stream& operator<<(binary_stream& w, const serializable<T>& s) 
    {
        s.serialize(w); return w;
    }
    template<class T> binary_stream& operator>>(binary_stream& r, serializable<T>& s) 
    {
        s.deserialize(r); return r;
    }
    //template<class T> string_serializer& operator<<(string_serializer& w, const serializable<T>& s) 
    //{
    //    s.serialize(w); return w;
    //}
    template<class T> strview& operator>>(strview& r, serializable<T>& s) 
    {
        s.deserialize(r); return r;
    }
    template<class T> string_serializer& operator>>(string_serializer& r, serializable<T>& s)
    {
        strview sv = r.view();
        s.deserialize(sv); return r;
    }
    //////////////////////////////////////////////////////////////////////////////////

} // namespace rpp

#endif // RPP_BINARY_SERIALIZER_H
