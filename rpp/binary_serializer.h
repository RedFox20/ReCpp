#pragma once
#ifndef RPP_BINARY_SERIALIZER_H
#define RPP_BINARY_SERIALIZER_H
#include "binary_stream.h"
#include "delegate.h"
//#include "binary_reader.h"
//#include "binary_writer.h"

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

    template<class T> struct member_serialize
    {
        using serialize_func   = void(*)(const T* inst, int offset, binary_stream& w);
        using deserialize_func = void(*)(T* inst,       int offset, binary_stream& r);
        int offset;
        strview name; // for named serialization such as json
        serialize_func   serialize;
        deserialize_func deserialize;
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
        }

        template<class U> void bind(U& elem) noexcept
        {
            member_serialize<T> m;
            m.offset    = int((char*)&elem - (char*)this);
            m.serialize = [](const T* inst, int offset, binary_stream& w)
            {
                U& var = *(U*)((byte*)inst + offset);
                w << var;
            };
            m.deserialize = [](T* inst, int offset, binary_stream& r)
            {
                U& var = *(U*)((byte*)inst + offset);
                r >> var;
            };
            members.emplace_back(move(m));
        }

        template<class U> void bind(strview name, U& elem) noexcept
        {
            member_serialize<T> m;
            m.offset    = int((char*)&elem - (char*)this);
            m.name      = name;
            m.serialize = [](T* inst, int offset, binary_writer& w)
            {
                U& var = *(U*)((byte*)inst + offset);
                w << var;
            };
            m.deserialize = [](T* inst, int offset, binary_reader& r)
            {
                U& var = *(U*)((byte*)inst + offset);
                r >> var;
            };
            members.emplace_back(move(m));
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
                memberInfo.serialize(inst, memberInfo.offset, w);
            }
        }

        void deserialize(binary_stream& r)
        {
            T* inst = static_cast<T*>(this);
            for (member_serialize<T>& memberInfo : members)
            {
                memberInfo.deserialize(inst, memberInfo.offset, r);
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

    //////////////////////////////////////////////////////////////////////////////////

} // namespace rpp

#endif // RPP_BINARY_SERIALIZER_H
