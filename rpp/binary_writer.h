#pragma once
#ifndef RPP_BINARY_WRITER_H
#define RPP_BINARY_WRITER_H
#include <string>
#include <vector>
#include <type_traits> // std::is_pod<T>
#include <cassert>     // assert
#include <cstdlib>     // malloc
#include <cstdio>      // fopen
#include "strview.h"
#include "sockets.h"

namespace rpp /* ReCpp */
{
    #ifndef RPP_BASIC_INTEGER_TYPEDEFS
    #define RPP_BASIC_INTEGER_TYPEDEFS
        typedef unsigned char    byte;
        typedef unsigned short   ushort;
        typedef unsigned int     uint;
        typedef __int64          int64;
        typedef unsigned __int64 uint64;
    #endif

    #ifndef RPP_MOVECOPY_MACROS_DEFINED
    #define RPP_MOVECOPY_MACROS_DEFINED
    // @note default MOVE copies raw types; beware of <owning> raw pointers and using this macro... it can cause double free
    #define MOVE(Class,value) Class(Class&&)=value;       Class&operator=(Class&&)=value;
    #define NOCOPY(Class)     Class(const Class&)=delete; Class&operator=(const Class&)=delete;
    #define NOCOPY_MOVE(Class)   MOVE(Class,default) NOCOPY(Class) 
    #define NOCOPY_NOMOVE(Class) MOVE(Class,delete)  NOCOPY(Class) 
    #endif

    /**
     * binary_writer mixin adapter interface
     */
    struct writer_base
    {
        virtual ~writer_base() noexcept = default;
        virtual void write(const void* data, int numBytes) noexcept = 0;
        virtual void flush() noexcept = 0;
    };

    /**
     * A generic data writer.
     * Implementation is defined by container template parameter.
     * Container class must properly implement its copy/move constructor rules
     * Write destination container implemention must define the following functions:
     *     char* data() const;
     *     uint  size() const;
     *     uint  available() const;
     *     void  clear();
     *     void  flush();
     *     void  write(const void* data, uint numBytes);
     *     template<class T> void write(const T& data);
     */
    template<class write_impl> struct binary_writer final : public writer_base, public write_impl
    {
        using write_impl::write_impl; // inherit base constructors

        /** @brief Writes raw data into the buffer */
        void write(const void* data, int numBytes) noexcept override {
            write_impl::write(data, numBytes);
        }
        /** @brief Flush any write buffers on this writer */
        void flush() noexcept override {
            write_impl::flush();
        }

        /** @brief Writes generic POD type data into the buffer */
        template<class T> binary_writer& write(const T& data) noexcept {
            write_impl::write(data);
            return *this;
        }
        /** @brief Appends data from other binary_writer to this one */
        template<class C> binary_writer& write(const binary_writer<C>& wb) noexcept {
            write(wb.data(), wb.size());
            return *this;
        }

        binary_writer& write_byte(byte value)     noexcept { return write(value); } /** @brief Writes a 8-bit unsigned byte into the buffer */
        binary_writer& write_short(short value)   noexcept { return write(value); } /** @brief Writes a 16-bit signed short into the buffer */
        binary_writer& write_ushort(ushort value) noexcept { return write(value); } /** @brief Writes a 16-bit unsigned short into the buffer */
        binary_writer& write_int(int value)       noexcept { return write(value); } /** @brief Writes a 32-bit signed integer into the buffer */
        binary_writer& write_uint(uint value)     noexcept { return write(value); } /** @brief Writes a 32-bit unsigned integer into the buffer */
        binary_writer& write_int64(int64 value)   noexcept { return write(value); } /** @brief Writes a 64-bit signed integer into the buffer */
        binary_writer& write_uint64(uint64 value) noexcept { return write(value); } /** @brief Writes a 64-bit unsigned integer into the buffer */

        /** @brief Write a length specified string to the buffer in the form of [uint16 len][data] */
        template<class Char> binary_writer& write_nstr(const Char* str, int len) noexcept {
            write_int(len).write(str, len * sizeof(Char)); return *this;
        }
        binary_writer& write(const strview& str)      noexcept { return write_nstr(str.str, str.len); }
        binary_writer& write(const std::string& str)  noexcept { return write_nstr(str.c_str(), (int)str.length()); }
        binary_writer& write(const std::wstring& str) noexcept { return write_nstr(str.c_str(), (int)str.length()); }

        /**
         * @brief Writes all the binary data inside the vector as [uint16 len] [ len * serializeFunc() ]
         * @note  Binary data layout depends on the user passed function serializeFunc() implementation
         * @note  May require operator<< if T is not POD type:
         *            binary_buffer& operator<<(binary_buffer& wb, const T& item);
         */
        template<class T, class U> binary_writer& write(const std::vector<T, U>& vec) noexcept {
            int count = (int)vec.size();
            write_int(count);
            const T* data = vec.data();
            if (std::is_pod<T>::value)
                write(data, sizeof(T) * count);
            else for (int i = 0; i < count; ++i)
                *this << data[i];
            return *this;
        }
    };
    // explicit operator<< overloads
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, const strview& v)      noexcept { return w.write(v); }
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, const std::string& v)  noexcept { return w.write(v); }
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, const std::wstring& v) noexcept { return w.write(v); }
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, bool v)   noexcept { return w.write(v); }
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, char v)   noexcept { return w.write(v); }
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, byte v)   noexcept { return w.write(v); }
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, short v)  noexcept { return w.write(v); }
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, ushort v) noexcept { return w.write(v); }
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, int v)    noexcept { return w.write(v); }
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, uint v)   noexcept { return w.write(v); }
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, int64 v)  noexcept { return w.write(v); }
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, uint64 v) noexcept { return w.write(v); }
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, float v)  noexcept { return w.write(v); }
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, double v) noexcept { return w.write(v); }
    template<class C, class T, class U> binary_writer<C>& operator<<(binary_writer<C>& w, const std::vector<T, U>& v) noexcept { return w.write(v); }
    template<class C> binary_writer<C>& operator<<(binary_writer<C>& w, binary_writer<C>& m(binary_writer<C>&)) noexcept { return m(w); }
    template<class C> binary_writer<C>& endl(binary_writer<C>& w) noexcept { w.flush(); return w; }





    /**
     * A composite writer utilizes first type for immediate writes (data Buffering)
     * and secondary type for flush writes (data Storage file/socket/etc).
     * Flush is automatically called during destruction and can be called if needed
     * to write data from buffer to storage.
     * Clear only clears the Buffer class data and does not affect storage
     * @note Buffer  class must automatically initialize itself (!)
     * @note Storage class constructors are exposed for initialization
     */
    template<class buffer, class storage> struct composite_write final
        : public storage, private buffer
    {
        using storage::storage; // inherit Storage class constructors
        ~composite_write() { flush(); }

        char*data()      const { return buffer::data(); }
        int  size()      const { return buffer::size(); }
        int  available() const { return buffer::available(); }
        void clear() { buffer::clear(); }
        void flush() { storage::write(data(), size()); clear(); }
        void write(const void* data, int numBytes) {
            if (available() < numBytes) flush(); // forced flush
            buffer::write(data, numBytes);
        }
        template<class T> void write(const T& data) {
            if (available() < sizeof(T)) flush(); // forced flush
            buffer::write(data);
        }
    };

    /**
     * A static array write buffer. Size of the write array does not change.
     * A small default array size is provided as 512 bytes.
     */
    template<unsigned Max = 512> struct array_write
    {
        uint Pos;       // current pos
        byte Buf[Max];  // buffer

        array_write() : Pos(0) {}
        NOCOPY_NOMOVE(array_write)

        char*data()      const { return (char*)Buf; }
        int  size()      const { return Pos; }
        int  available() const { return Max - Pos; }
        void clear() { Pos = 0; *Buf = 0; }
        void flush() { }
        void write(const void* data, uint numBytes) {
            assert(Pos + numBytes <= Max);
            memcpy(Buf + Pos, data, numBytes), Pos += numBytes;
        }
        template<class T> void write(const T& data) {
            assert(Pos + sizeof(T) <= Max);
            *(T*)&Buf[Pos] = data, Pos += sizeof(T);
        }
    };

    /**
     * A static view write buffer. A basic wrapper similar to array_view
     * The actual data buffer is allocated somewhere else and this class merely
     * holds a pointer to the data.
     */
    struct view_write
    {
        char* Buf; // weak data reference
        int   Pos; // current pos
        int   Max; // total buffer capacity

        view_write() : Buf(0), Pos(0), Max(0) {}
        view_write(void* buf, int len) : Buf((char*)buf), Pos(0), Max(len) {}
        explicit view_write(std::vector<byte>& v) : Buf((char*)v.data()), Pos(0), Max((int)v.size()) {}
        NOCOPY_MOVE(view_write)

        char*data()      const { return Buf; }
        int  size()      const { return Pos; }
        int  available() const { return Max - Pos; }
        void clear() { Pos = 0; *Buf = 0; }
        void flush() { }
        void write(const void* data, int numBytes) {
            assert(Pos + numBytes <= Max);
            memcpy(Buf + Pos, data, numBytes), Pos += numBytes;
        }
        template<class T> void write(const T& data) {
            assert(Pos + sizeof(T) <= Max);
            *(T*)&Buf[Pos] = data, Pos += sizeof(T);
        }
    };

    /**
     * A dynamic write buffer, growth is amortized and dynamic, size grows aligned to 512-bytes.
     * If initial capacity is set, a buffer will be preallocated precisely (!).
     */
    struct buffer_write
    {
        char* Buf; // owning data pointer
        int   Pos; // current pos
        int   Cap; // total data capacity

        buffer_write() : Buf(0), Pos(0), Cap(0) {}
        explicit buffer_write(int capacity) : Buf((char*)malloc(capacity)), Pos(0), Cap(capacity) {}
        ~buffer_write() { if (Buf) free(Buf); }

        NOCOPY(buffer_write)

        buffer_write(buffer_write&& fwd) : Buf(fwd.Buf), Pos(fwd.Pos), Cap(fwd.Cap) {
            fwd.Buf = nullptr, fwd.Pos = 0, fwd.Cap = 0;
        }
        buffer_write& operator=(buffer_write&& fwd) {
            swap(Buf, fwd.Buf);
            swap(Pos, fwd.Pos);
            swap(Cap, fwd.Cap);
            return *this;
        }

        char*data() const { return Buf; }
        int  size() const { return Pos; }
        int  available() const { return 0xffffffff; } // buffer writer always has enough room, because it can resize
        void clear() { Pos = 0; *Buf = 0; }
        void flush() { }
        void write(const void* data, int numBytes) {
            reserve(numBytes);
            memcpy(&Buf[Pos], data, numBytes), Pos += numBytes;
        }
        template<class T> void write(const T& data) {
            reserve(sizeof(T));
            *(T*)&Buf[Pos] = data, Pos += sizeof(T);
        }
        void reserve(int numBytes) { // reserve space for N extra bytes
            if (numBytes > (Cap - Pos)) { // n > (remaining free bytes)
                int cap = Cap + numBytes;
                if (int rem = (cap % 512)) cap += 512 - rem; // always 512 aligned
                Buf = (char*)realloc(Buf, Cap = cap);
            }
        }
    };

    /**
     * Writes binary data into a file, straight through C FILE* API
     * FILE is opened with "wb" by default, clear() will reopen the file with "wb"
     */
    struct file_write
    {
        FILE* File;
        std::string Path;

        file_write(std::string&& path, const char* mode = "wb") : File(fopen(path.c_str(), mode)), Path(move(path)) {}
        file_write(const std::string& path, const char* mode = "wb") : File(fopen(path.c_str(), mode)), Path(path) {}
        ~file_write() { if (File) fclose(File); }

        NOCOPY(file_write)
        
        file_write(file_write&& fwd) : File(fwd.File), Path(move(fwd.Path)) {
            fwd.File = nullptr;
        }
        file_write& operator=(file_write&& fwd) {
            swap(File, fwd.File);
            swap(Path, fwd.Path);
            return *this;
        }

        bool good()      const { return File != 0; }
        operator bool()  const { return File != 0; }
        char*data()      const { return nullptr; }
        int  size()      const { return ftell(File); }
        int  available() const { return 0xffffffff; } // file has unlimited size
        void clear() { File = freopen(Path.c_str(), "wb", File); }
        void flush() { fflush(File); }
        void write(const void* data, int numBytes) { fwrite(data, numBytes, 1, File); }
        template<class T> void write(const T& data) { fwrite(&data, sizeof(T), 1, File); }
    };

    /**
     * Writes binary data into a socket, socket is kept as a pointer.
     */
    struct socket_write
    {
        rpp::socket* Socket; // as pointer to allow Move

        explicit socket_write(rpp::socket& s) : Socket(&s) {}
        NOCOPY_MOVE(socket_write)

        bool good()      const { return Socket && Socket->good(); }
        operator bool()  const { return Socket && Socket->good(); }
        char*data()      const { return nullptr; }
        int  size()      const { return 0; }
        int  available() const { return 0xffffffff; } // network is unlimited
        void clear() { }
        void flush() { Socket->flush(); }
        void write(const void* data, int numBytes) { Socket->send(data, numBytes); }
        template<class T> void write(const T& data) { Socket->send(&data, sizeof(T)); }
    };


    //////////////////// some predefined compositions ////////////////////


    /** @brief Writes data into a fixed sized array. Default size 512 bytes.  */
    template<int SIZE = 512> using array_writer = binary_writer<array_write<SIZE>>;
    /** @brief Writes data into an array view. Array size depends on its initialized view size. */
    using view_writer   = binary_writer<view_write>;
    /** @brief Writes data into a dynamically growing buffer. Buffer grows aligned to 512 bytes. */
    using buffer_writer = binary_writer<buffer_write>;
    /** @brief Writes data to file using C FILE* API. */
    using file_writer   = binary_writer<file_write>;
    /** @brief Writes data directly to an rpp::socket */
    using socket_writer = binary_writer<socket_write>;
    /**
     * @brief A stream writer utilizes a primary buffer class and a backing storage class
     *        All data is buffered by the buffer class, some options for buffer:
     *		      array_write<512> - writes into a fixed sized array;        flush() if full
     *		      view_write       - writes to an array view of some buffer; flush() if full
     *		      buffer_write     - writes to a dynamically growing buffer; only explicit flush() or << endl;
     *		  Storage class is only used for data flushing, some options:
     *		      file_write       - flushes to a C FILE
     *		      socket_write     - flushes to an rpp::socket
     */
    template<class buffer, class storage> using stream_writer = binary_writer<composite_write<buffer, storage>>;


    template<int SIZE = 512>
    using socket_arraystream_writer  = stream_writer<array_write<SIZE>, socket_write>;
    using socket_viewstream_writer   = stream_writer<view_write, socket_write>;
    using socket_bufferstream_writer = stream_writer<buffer_write, socket_write>;
    template<int SIZE = 512>
    using file_arraystream_writer  = stream_writer<array_write<SIZE>, file_write>;
    using file_viewstream_writer   = stream_writer<view_write, file_write>;
    using file_bufferstream_writer = stream_writer<buffer_write, file_write>;


} // namespace rpp

#endif // RPP_BINARY_WRITER_H
