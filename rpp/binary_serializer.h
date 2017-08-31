#pragma once
#ifndef RPP_BINARY_SERIALIZER_H
#define RPP_BINARY_SERIALIZER_H
#include "binary_readwrite.h"
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

        NOINLINE binary_writer& write(binary_writer& w) const noexcept;
        NOINLINE binary_reader& read(binary_reader& r) noexcept;

        /** @brief Returns the serialization layout size */
        NOINLINE int layout_size() const noexcept;
    };

    inline binary_writer& operator<<(binary_writer& w, const binary_serializer& s) 
    {
        return s.write(w); 
    }
    inline binary_reader& operator>>(binary_reader& r, binary_serializer& s) 
    {
        return s.read(r); 
    }

    //////////////////////////////////////////////////////////////////////////////////

    template<class T> constexpr int size_of(const T& v); // forward declr.

    namespace detail
    {
        template<class T> struct size_of
        {
            static constexpr int value(const T&) noexcept { return sizeof(T); };
        };

        template<class T> struct size_of<vector<T>>
        {
            static int value(const vector<T>& v) noexcept
            {
                int size = sizeof(int);
                for (const T& item : v) size += rpp::size_of(item);
                return size;
            }
        };

        template<class Char> struct size_of<basic_string<Char>>
        {
            static int value(const basic_string<Char>& s) noexcept
            {
                return sizeof(int) + sizeof(Char) * (int)s.size();
            }
        };

        template<> struct size_of<strview>
        {
            static int value(const strview& s) noexcept
            {
                return sizeof(int) + s.len * sizeof(char);
            }
        };

        template<class ...T> struct size_of<tuple<T...>>
        {
            static constexpr int size(const tuple<T...>& t)
            {
                return 0;
            }

            //template<int N> static const int size(const tuple<T...>& t) noexcept
            //{
            //    return rpp::size_of(get<N - 1>(t)) + size<N-1>(t);
            //}

            static int value(const tuple<T...>& t) noexcept
            {
                return 0;
                //return size<sizeof...(T)>(t);
            }
        };
    }

    /**
     * Runtime size of the objects once serialized
     */
    template<class T> constexpr int size_of(const T& v)
    {
        return detail::size_of<T>::value(v);
    }

    
    template<class T, class A> int auto_expand(const T& obj = { A{} })
    {
        std::tuple<A> t1;
        return 1;
    }

    template<class T, class A, class B> int auto_expand(const T& obj = { A{}, B{} })
    {
        return 2;
    }

    template<class T> struct member_serialize
    {
        using serialize_func   = void(*)(T* inst, int offset, binary_writer& w);
        using deserialize_func = void(*)(T* inst, int offset, binary_reader& r);
        int offset;
        serialize_func   serialize;
        deserialize_func deserialize;
    };

    template<class T> struct serializable
    {
        static vector<member_serialize<T>> members;
        static bool introspection_complete;

        serializable()
        {
            if (!introspection_complete)
                static_cast<T*>(this)->introspect();
        }

        template<class U> void bind(U& elem) noexcept
        {
            member_serialize<T> m;
            m.offset    = int((char*)&elem - (char*)this);
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

        void serialize(binary_writer& w)
        {
            T* inst = static_cast<T*>(this);
            for (member_serialize<T>& memberInfo : members)
            {
                memberInfo.serialize(inst, memberInfo.offset, w);
            }
        }

        void deserialize(binary_reader& r)
        {
            T* inst = static_cast<T*>(this);
            for (member_serialize<T>& memberInfo : members)
            {
                memberInfo.deserialize(inst, memberInfo.offset, r);
            }
        }
    };

    template<class T> vector<member_serialize<T>> serializable<T>::members;
    template<class T> bool serializable<T>::introspection_complete;

    //////////////////////////////////////////////////////////////////////////////////

} // namespace rpp

#endif // RPP_BINARY_SERIALIZER_H
