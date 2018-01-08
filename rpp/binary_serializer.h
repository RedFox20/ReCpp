#pragma once
/**
 * Efficient binary serialization/deserialization, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#ifndef RPP_BINARY_SERIALIZER_H
#define RPP_BINARY_SERIALIZER_H 1
#endif
#include "binary_stream.h"

namespace rpp
{
    //////////////////////////////////////////////////////////////////////////////////

    template<class T> struct member_serialize
    {
        using bserialize_func   = void(*)(const T* inst, int offset, binary_stream& w);
        using bdeserialize_func = void(*)(T* inst,       int offset, binary_stream& r);
        using sserialize_func   = void(*)(const T* inst, int offset, string_buffer& w);
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

        template<class U> static void string_serialize(const T* inst, int offset, string_buffer& w)
        {
            U& var = *(U*)((byte*)inst + offset);
            w.write(var);
        }
        template<class U> static void string_deserialize(T* inst, int offset, strview token)
        {
            U& var = *(U*)((byte*)inst + offset);
            operator>>(token, var);
            //token >> var;
        }


    protected:

        template<class U> void bind(U& elem) noexcept
        {
            member_serialize<T> m;
            m.offset       = int((char*)&elem - (char*)this);
            m.bserialize   = &serializable::template binary_serialize<U>;
            m.bdeserialize = &serializable::template binary_deserialize<U>;
            m.sserialize   = &serializable::template string_serialize<U>;
            m.sdeserialize = &serializable::template string_deserialize<U>;
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

    public:

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
        void serialize(string_buffer& w) const
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
    //template<class T> string_buffer& operator<<(string_buffer& w, const serializable<T>& s) 
    //{
    //    s.serialize(w); return w;
    //}
    template<class T> strview& operator>>(strview& r, serializable<T>& s) 
    {
        s.deserialize(r); return r;
    }
    template<class T> string_buffer& operator>>(string_buffer& r, serializable<T>& s)
    {
        strview sv = r.view();
        s.deserialize(sv); return r;
    }

    //////////////////////////////////////////////////////////////////////////////////
} // namespace rpp
