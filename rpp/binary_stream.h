#pragma once
/**
 * Efficient binary streams, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "strview.h"
#ifndef RPP_BINARY_READWRITE_NO_SOCKETS
#  include <mutex>
#  include "sockets.h"
#endif
#ifndef RPP_BINARY_READWRITE_NO_FILE_IO
#  include "file_io.h"
#endif

#if _MSC_VER
#  pragma warning(disable: 4251) // class 'std::*' needs to have dll-interface to be used by clients of struct 'rpp::*'
#endif

#ifndef RPPAPI
#  if _MSC_VER
#    define RPPAPI __declspec(dllexport)
#  else // clang/gcc
#    define RPPAPI __attribute__((visibility("default")))
#  endif
#endif

namespace rpp /* ReCpp */ 
{
    ////////////////////////////////////////////////////////////////////////////

    #ifndef RPP_BASIC_INTEGER_TYPEDEFS
    #define RPP_BASIC_INTEGER_TYPEDEFS
        #ifdef _LIBCPP_STD_VER
        #  define _HAS_STD_BYTE (_LIBCPP_STD_VER > 16)
        #elif !defined(_HAS_STD_BYTE)
        #  define _HAS_STD_BYTE 0
        #endif
        #if !_HAS_STD_BYTE
            using byte = unsigned char;
        #else
            using byte = std::byte;
        #endif
        using ushort = unsigned short;
        using uint   = unsigned int;
        using ulong  = unsigned long;
        using int64  = long long;
        using uint64 = unsigned long long;
    #endif

    #ifndef NOCOPY_NOMOVE
    #define NOCOPY_NOMOVE(Class)                                   \
                          Class(Class&&)                 = delete; \
                          Class(const Class&)            = delete; \
                          Class& operator=(Class&&)      = delete; \
                          Class& operator=(const Class&) = delete;
    #endif


    #ifndef RPP_MINMAX_DEFINED
    #define RPP_MINMAX_DEFINED
        template<class T> T max(T a, T b) { return a > b ? a : b; }
        template<class T> T min(T a, T b) { return a < b ? a : b; }
        template<class T> T min3(T a, T b, T c) { return a < b ? (a<c?a:c) : (b<c?b:c); }
    #endif
    

    ////////////////////////////////////////////////////////////////////////////


    /**
     * @brief A generic stream source.
     */
    struct RPPAPI stream_source
    {
        stream_source() = default;
        virtual ~stream_source() noexcept = default;

        /**
         * @return TRUE if stream is open, FALSE if the stream is closed
         */
        virtual bool stream_good() const noexcept = 0;

        /**
         * Writes a block of data to the underlying storage target or network device
         * @return Number of bytes actually written. Must be [numBytes] or <= 0 on failure
         */
        virtual int stream_write(const void* data, int numBytes) = 0;

        /**
         * Flush all read/write buffers on the stream
         */
        virtual void stream_flush() = 0;

        /**
         * @brief Reads bytes directly from the underlying stream
         * @param dst Destination buffer to read into
         * @param max Maximum number of characters to peek
         * @return Number of bytes actually read; 0 if nothing was read (no data); -1 if stream failed/closed
         */
        virtual int stream_read(void* dst, int max) noexcept = 0;

        /**
         * @return Number of bytes available in the stream for future read operations
         */
        virtual int stream_available() const noexcept { return 0; }

        /**
         * Peeks the stream for the next few bytes. Not all streams can be peeked, so this implementation is optional.
         * @param dst Destination buffer to read into
         * @param max Maximum number of characters to peek
         * @return Number of bytes actually peeked. Not all streams can be peeked, in which case result is 0;
         */
        virtual int stream_peek(void* dst, int max) noexcept
        {
            (void)dst;
            (void)max;
            return 0;
        }

        /**
         * @param numBytesToSkip Number of bytes to skip from the read stream
         */
        virtual void stream_skip(int numBytesToSkip) noexcept = 0;

        // stream_source is pure virtual and shouldn't be copied or moved
        NOCOPY_NOMOVE(stream_source)
    };


    ////////////////////////////////////////////////////////////////////////////


    /**
     * @brief A generalized buffered binary stream.
     *        Instance of stream_source defines the implementation: file, socket, ...
     *        
     *        For streams, buffering can be disabled by passing in capacity: 0,
     *        which means all data is piped into stream source.
     *        
     * @warning There is no automatic flushing during normal write operations, so for
     *          large binary streams, you will need to manually flush the stream with .flush() or << endl
     *          The stream buffer is flushed in the destructor.
     * @code
     *     if (rpp::file f { filename })
     *     {
     *         file_writer fs { f };
     *         fs << "binary string";
     *     
     *         fs << std::endl; // example: manual flush
     *         fs.flush();      // example: another manual flush
     *     
     *     } // f is autoflushed
     * @endcode
     * 
     * @warning Binary Stream provides both read and write operations, on the same underlying
     *          buffer, so you can immediatelly read anything that was written.
     *          In the case of sockets this makes little sense, so for bidirectional
     *          communication it makes sense to use two instances instead of one:
     * @code
     *     socket s = socket::connect_to(...);
     *     socket_stream out {s};
     *     socket_stream in  {s};
     *     
     *     out << "Hello!" << endl;
     *     
     *     if (s.select(2000, SF_Read) // wait 2000ms for response
     *     {
     *         string response;
     *         in >> response;
     *     }
     * @endcode
     *         
     */
    class RPPAPI binary_stream
    {
    public:
        // Small Buffer Optimization size:
        static constexpr int SBSize = 512;

    private:

        int ReadPos  = 0;  // read head position in the buffer
        int WritePos = 0;  // write head position in the buffer
        int End = 0;       // end of data
        int Cap = SBSize;  // current buffer capacity
        char* Ptr;         // pointer to current buffer, either this->Buf or a dynamically allocated one
        stream_source* Src = nullptr;
        char  Buf[SBSize];

    public:

        /**
         * Creates a binary_stream with a default read/write buffer of size [binary_stream::SBSize]
         */
        explicit binary_stream(stream_source* src = nullptr) noexcept;

        /**
         * Create binary writer with buffer capacity. If capacity == 0, then no buffering is used.
         */
        explicit binary_stream(int capacity, stream_source* src = nullptr) noexcept;

        virtual ~binary_stream() noexcept;

        // binary_stream is pure virtual and shouldn't be copied or moved
        NOCOPY_NOMOVE(binary_stream)

        void disable_buffering();

        const char* data()  const { return &Ptr[ReadPos]; }
        char*       data()        { return &Ptr[ReadPos]; }
        const char* begin() const { return &Ptr[ReadPos]; }
        const char* end()   const { return &Ptr[End]; }
        int readpos()      const { return ReadPos; }
        int writepos()     const { return WritePos; }
        // @return Number of bytes in the read buffer
        int size()      const { return End - ReadPos; }
        int capacity()  const { return Cap; }
        strview view()   const { return { data(), (int)size() }; }

        // @return Total buffered bytes and available stream bytes: 
        //         size() + stream_available()
        int available() const noexcept;

        /**
         * Sets the buffer position and size to 0; no data is flushed
         */
        void clear();

        /**
         * Rewinds the Read/Write head to a specific position in the BUFFER.
         * All subsequent read/write operations will begin from the given position.
         * @note New pos is range checked and clamped
         */
        void rewind(int pos = 0);

        /**
         * @return TRUE if this stream is open and data is available
         */
        bool good() const;
        explicit operator bool() const { return good(); }

        /** 
         * Flushes the buffer and changes the size of write buffer
         * Setting this to 0 will disable buffering. Setting it to <= binary_stream::SBSize has no effect
         */
        void reserve(int capacity);

        /** 
         * @brief Flush write buffer on this writer.
         */
        void flush();

    private:
        NOINLINE void ensure_space(int numBytes);

    public:
        ////////////////////// ----- Writer Fields ----- //////////////////////////

        /** @brief Writes raw data into the buffer */
        binary_stream& write(const void* data, int numBytes);

        /** @brief Writes object of type T into the buffer */
        template<class T> binary_stream& write(const T& data)
        {
            ensure_space((int)sizeof(T));
            *(T*)&Ptr[WritePos] = data;
            WritePos += (int)sizeof(T);
            End      += (int)sizeof(T);
            return *this;
        }

    private:
        void unsafe_write(const void* data, int numBytes);
        template<class T> void unsafe_write(const T& data)
        {
            *(T*)&Ptr[WritePos] = data;
            WritePos += (int)sizeof(T);
            End      += (int)sizeof(T);
        }

    public:

        /** @brief Appends data from other binary_writer to this one */
        binary_stream& write(const binary_stream& w) { return write(w.data(), w.size()); }
        binary_stream& write_byte(uint8_t value)  { return write(value); } /** @brief Writes a 8-bit unsigned byte into the buffer */
        binary_stream& write_short(short value)   { return write(value); } /** @brief Writes a 16-bit signed short into the buffer */
        binary_stream& write_ushort(ushort value) { return write(value); } /** @brief Writes a 16-bit unsigned short into the buffer */
        binary_stream& write_int(int value)       { return write(value); } /** @brief Writes a 32-bit signed integer into the buffer */
        binary_stream& write_uint(uint value)     { return write(value); } /** @brief Writes a 32-bit unsigned integer into the buffer */
        binary_stream& write_int64(int64 value)   { return write(value); } /** @brief Writes a 64-bit signed integer into the buffer */
        binary_stream& write_uint64(uint64 value) { return write(value); } /** @brief Writes a 64-bit unsigned integer into the buffer */

        using strlen_t = int32_t;

        /** @brief Write a length specified string to the buffer in the form of [strlen_t len][data] */
        template<class Char> binary_stream& write_nstr(const Char* str, int len) {
            return write<strlen_t>(len).write((void*)str, len * (int)sizeof(Char));
        }
        binary_stream& write(const strview& str)      { return write_nstr(str.str, str.len); }
        binary_stream& write(const std::string& str)  { return write_nstr(str.c_str(), (int)str.length()); }
        binary_stream& write(const std::wstring& str) { return write_nstr(str.c_str(), (int)str.length()); }

        template<class T>
        static constexpr bool is_trivial_type = std::is_default_constructible_v<T>
                                             && std::is_trivially_destructible_v<T>;

        template<class T, class A>
        binary_stream& write(const std::vector<T, A>& v)
        {
            int n = (int)v.size();
            int cap = min(4 + n * (int)sizeof(T), 1024 * 1024); // fuzzy capacity
            ensure_space(cap);

            if constexpr(is_trivial_type<T>)
            {
                unsafe_write<int>(n);
                unsafe_write(v.data(), n * (int)sizeof(T));
            }
            else
            {
                write<int>(n);
                for (const T& item : v)
                    *this << item; // @note ADL fails easily here, use write_vector with custom writer instead
            }
            return *this;
        }

        template<class T, class A, class Writer>
        binary_stream& write(const std::vector<T, A>& v, const Writer& writer)
        {
            int n = (int)v.size();
            int cap = min(4 + n * (int)sizeof(T), 1024*1024); // fuzzy capacity
            ensure_space(cap);

            write<int>(n);
            for (const T& item : v)
                writer(*this, item);
            return *this;
        }

        ////////////////////// ----- Reader Fields ----- //////////////////////////

    private:
        int unsafe_buffer_fill();
        int unsafe_buffer_read(void* dst, int bytesToRead);
        int fragmented_read(void* dst, int bytesToRead);

        template<class T> int unsafe_buffer_read(T& dst)
        {
            dst = *(T*)&Ptr[ReadPos];
            ReadPos += (int)sizeof(T);
            return (int)sizeof(T);
        }

    public:
        int read(void* dst, int bytesToRead);
        template<class T> int read(T& dst)
        {
            if (size() >= (int)sizeof(T))
                return unsafe_buffer_read<T>(dst); // best case, all from buffer
            return fragmented_read(&dst, (int)sizeof(T)); // fallback partial read
        }

        int peek(void* dst, int bytesToPeek);
        
        template<class T> int peek(T& dst)
        {
            int avail = size();
            if (avail <= 0)
            {
                if (!Src) return 0;
                avail = unsafe_buffer_fill(); // fill before peek if possible
            }
            if (avail < (int)sizeof(T))
                return 0;
            dst = *(T*)&Ptr[ReadPos];
            return (int)sizeof(T);
        }
        
        /** @brief Skips data in the buffer and in the stream */
        void skip(int n);

        /** @brief Attempts to undo a previous read from the buffer. Not very reliable. */
        void undo(int n);

        /** @brief Reads generic POD type data and returns it */
        template<class T> T read() { T out{}; read(out); return out; }
        /** @brief Peeks generic POD type data and returns it without advancing the streampos */
        template<class T> T peek() { T out{}; peek(out); return out; }

        uint8_t read_byte()   { return read<uint8_t>();} /** @brief Reads a 8-bit unsigned byte   */
        short   read_short()  { return read<short>();  } /** @brief Reads a 16-bit signed short   */
        ushort  read_ushort() { return read<ushort>(); } /** @brief Reads a 16-bit unsigned short */
        int     read_int()    { return read<int>();    } /** @brief Reads a 32-bit signed integer */
        uint    read_uint()   { return read<uint>();   } /** @brief Reads a 32-bit unsigned integer */
        int64   read_int64()  { return read<int64>();  } /** @brief Reads a 64-bit signed integer   */
        uint64  read_uint64() { return read<uint64>(); } /** @brief Reads a 64-bit unsigned integer */

        uint8_t peek_byte()   { return peek<uint8_t>();}/** @brief Peeks a 8-bit unsigned byte without advancing the streampos   */
        short   peek_short()  { return peek<short>();  } /** @brief Peeks a 16-bit signed short without advancing the streampos   */
        ushort  peek_ushort() { return peek<ushort>(); } /** @brief Peeks a 16-bit unsigned short without advancing the streampos */
        int     peek_int()    { return peek<int>();    } /** @brief Peeks a 32-bit signed integer without advancing the streampos */
        uint    peek_uint()   { return peek<uint>();   } /** @brief Peeks a 32-bit unsigned integer without advancing the streampos */
        int64   peek_int64()  { return peek<int64>();  } /** @brief Peeks a 64-bit signed integer without advancing the streampos   */
        uint64  peek_uint64() { return peek<uint64>(); } /** @brief Peeks a 64-bit unsigned integer without advancing the streampos */

        /** @brief Reads a length specified string to the std::string in the form of [strlen_t len][data] */
        template<class Char> binary_stream& read(std::basic_string<Char>& str) {
            int n = min<strlen_t>(read<strlen_t>(), available());
            str.resize(size_t(n));
            read((void*)str.data(), (int)sizeof(Char) * n);
            return *this;
        }
        /** @brief Reads a length specified string to the dst buffer in the form of [strlen_t len][data] and returns actual length */
        template<class Char> int read_nstr(Char* dst, int maxLen) {
            int n = min3<strlen_t>(read<strlen_t>(), size(), maxLen);
            read((void*)dst, (int)sizeof(Char) * n);
            return n;
        }
        /** @brief Peeks a length specified string to the std::string in the form of [strlen_t len][data] */
        template<class Char> binary_stream& peek(std::basic_string<Char>& str) {
            int n = min<int>(read<strlen_t>(), size());
            str.resize(size_t(n));
            peek((void*)str.data(), (int)sizeof(Char) * n);
            undo(sizeof(strlen_t)); // undo read_int
            return *this;
        }
        /** @brief Peeks a length specified string to the dst buffer in the form of [strlen_t len][data] and returns actual length */
        template<class Char> int peek_nstr(Char* dst, int maxLen) {
            int n = min3<int>(read<strlen_t>(), size(), maxLen);
            n = peek((void*)dst, (int)sizeof(Char) * n) / (int)sizeof(Char);
            undo(sizeof(strlen_t)); // undo read_int
            return n;
        }

        std::string  read_string()  { std::string  s; read(s); return s; } /** @brief Reads a length specified [strlen_t len][data] string */
        std::wstring read_wstring() { std::wstring s; read(s); return s; } /** @brief Reads a length specified [strlen_t len][data] wstring */
        std::string  peek_string()  { std::string  s; peek(s); return s; } /** @brief Peeks a length specified [strlen_t len][data] string */
        std::wstring peek_wstring() { std::wstring s; peek(s); return s; } /** @brief Peeks a length specified [strlen_t len][data] wstring */
        strview      peek_strview(); /** @brief Peeks a length specified [strlen_t len][data] string view */
    
        template<class T, class A>
        binary_stream& read_vector(std::vector<T, A>& out)
        {
            int n = read_int();
            out.clear();
            if constexpr(is_trivial_type<T>)
            {
                out.resize(size_t(n));
                read(out.data(), n * (int)sizeof(T));
            }
            else
            {
                out.reserve(size_t(n));
                for (int i = 0; i < n; ++i) {
                #if _MSC_VER
                    *this >> out.emplace_back();
                #else
                    out.emplace_back();
                    *this >> out.back();
                #endif
                }
            }
            return *this;
        }

        template<class T, class A, class Reader>
        binary_stream& read_vector(std::vector<T, A>& out, const Reader& reader)
        {
            int n = read_int();
            out.clear();
            out.reserve(size_t(n));
            for (int i = 0; i < n; ++i)
                reader(*this, out.emplace_back());
            return *this;
        }
    };

    // explicit operator<< overloads
    inline binary_stream& operator<<(binary_stream& w, const strview& v)      { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, const std::string& v)  { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, const std::wstring& v) { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, bool v)   { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, char v)   { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, uint8_t v){ return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, short v)  { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, ushort v) { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, int v)    { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, uint v)   { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, int64 v)  { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, uint64 v) { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, float v)  { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, double v) { return w.write(v); }
    template<class T>
    binary_stream& operator<<(binary_stream& w, const std::vector<T>& v)
    {
        return w.write(v);
    }
    inline binary_stream& operator<<(binary_stream& w, binary_stream& m(binary_stream&)) { return m(w); }
    inline binary_stream& endl(binary_stream& w) { w.flush(); return w; }

    inline binary_stream& operator>>(binary_stream& r, strview& v)      { r.peek(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, std::string& v)  { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, std::wstring& v) { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, bool& v)   { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, char& v)   { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, uint8_t& v){ r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, short& v)  { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, ushort& v) { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, int& v)    { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, uint& v)   { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, int64& v)  { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, uint64& v) { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, float& v)  { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, double& v) { r.read(v); return r; }
    template<class T, class A>
    binary_stream& operator>>(binary_stream& r, std::vector<T, A>& out)
    {
        return r.read_vector(out);
    }
    inline binary_stream& operator>>(binary_stream& r, binary_stream& m(binary_stream&)) { return m(r); }



    ////////////////////////////////////////////////////////////////////////////


    /**
     * Special case of a binary stream, which doesn't flush any of the data
     */
    class RPPAPI binary_buffer : public binary_stream
    {
        using binary_stream::binary_stream;
    };


    ////////////////////////////////////////////////////////////////////////////


#ifndef RPP_BINARY_READWRITE_NO_SOCKETS


    /**
     * A generic binary socket writer. For UDP sockets, use socket::bind to set the destination ipaddress.
     */
    class RPPAPI socket_writer : public binary_stream, protected stream_source
    {
        rpp::socket* Sock = nullptr;
    public:
        socket_writer() noexcept : binary_stream{this} {}
        explicit socket_writer(rpp::socket& sock)      noexcept : binary_stream{this},           Sock(&sock) {}
        socket_writer(rpp::socket& sock, int capacity) noexcept : binary_stream{ capacity, this }, Sock(&sock) {}
        ~socket_writer() noexcept;
        void set_socket(rpp::socket& sock) noexcept { Sock = &sock; }
        NOCOPY_NOMOVE(socket_writer)

        bool stream_good() const noexcept override { return Sock && Sock->good(); }
        int stream_write(const void* data, int numBytes) noexcept override;
        void stream_flush() noexcept override;

        // does not support read operations
        int stream_read(void* dst, int max) noexcept override { (void)dst; (void)max; return 0; }
        int stream_peek(void* dst, int max) noexcept override { (void)dst; (void)max; return 0; }
        void stream_skip(int n) noexcept override { (void)n; }
    };


    /**
     * A generic binary socket reader. For UDP sockets the remote address can be inspected
     * via socket_reader::addr()
     */
    class RPPAPI socket_reader : public binary_stream, protected stream_source
    {
        rpp::socket* Sock = nullptr;
        rpp::ipaddress Addr;
    public:
        socket_reader() noexcept : binary_stream{this} {}
        explicit socket_reader(rpp::socket& sock)      noexcept : binary_stream{this},           Sock(&sock) {}
        socket_reader(rpp::socket& sock, int capacity) noexcept : binary_stream{capacity, this}, Sock(&sock) {}
        ~socket_reader() noexcept;
        void set_socket(rpp::socket& sock) noexcept { Sock = &sock; }
        NOCOPY_NOMOVE(socket_reader)

        const rpp::ipaddress& addr() const noexcept { return Addr; }
        bool stream_good() const noexcept override { return Sock && Sock->good(); }

        // does not support write operations
        int stream_write(const void* data, int numBytes) noexcept override { (void)data; (void)numBytes; return 0; }

        void stream_flush() noexcept override;
        int stream_read(void* dst, int max) noexcept override;
        int stream_peek(void* dst, int max) noexcept override;
        void stream_skip(int n) noexcept override;
    };


    /**
     * Almost identical to lock_guard, but allows move semantics
     */
    template<class Mutex = std::mutex> struct scoped_guard
    {
        Mutex* Mut;
        explicit scoped_guard(Mutex& m) : Mut(&m) { m.lock(); }
        ~scoped_guard() noexcept { if (Mut) Mut->unlock(); }

        scoped_guard(scoped_guard&& fwd) noexcept : Mut(fwd.Mut) { fwd.Mut = 0; }
        scoped_guard& operator=(scoped_guard&& fwd) noexcept {
            std::swap(Mut, fwd.Mut);
            return *this;
        }
        scoped_guard(const scoped_guard&)            = delete;
        scoped_guard& operator=(const scoped_guard&) = delete;
    };


    /**
     * A simple socket_writer wrapper that provides a lock_guard facility
     * for writing to the socket_writer buffer across multiple threads
     *
     * @warning Write operations may cause a flush(), so make sure to use nonblocking sockets
     *          otherwise you will experience long lock times
     */
    class RPPAPI shared_socket_writer : public socket_writer
    {
    protected:
        std::mutex Mutex;
    public:
        using socket_writer::socket_writer;

        // Acquire a scoped lock_guard on this socket writer's mutex
        // Ex:     auto&& lock = writer.guard();
        //
        scoped_guard<std::mutex> guard() noexcept {
            return scoped_guard<std::mutex>{ Mutex };
        }
    };


    /**
     * A simple socket_reader wrapper that provides a lock_guard facility
     * for reading from the socket_reader buffer across multiple threads
     *
     * @warning Read operations may cause a buffer fill event, so make
     *          sure the socket has enough data available to continue
     */
    class RPPAPI shared_socket_reader : public socket_reader
    {
    protected:
        std::mutex Mutex;
    public:
        using socket_reader::socket_reader;

        // Acquire a scoped lock_guard on this socket reader's mutex
        // Ex:     auto&& lock = reader.guard();
        //
        scoped_guard<std::mutex> guard() noexcept {
            return scoped_guard<std::mutex>{ Mutex };
        }
    };


#endif

    ////////////////////////////////////////////////////////////////////////////

#ifndef RPP_BINARY_READWRITE_NO_FILE_IO

    /**
     * A generic binary file writer. This is not the best for small inputs, but excels with huge contiguous streams
     */
    class RPPAPI file_writer : public binary_stream, protected stream_source
    {
        rpp::file* File = nullptr;
    public:
        file_writer() noexcept : binary_stream{this} {}
        explicit file_writer(rpp::file& file)      noexcept : binary_stream{this}, File(&file) {}
        file_writer(rpp::file& file, int capacity) noexcept : binary_stream{capacity, this}, File(&file) {}
        ~file_writer() noexcept;
        void set_file(rpp::file& file) noexcept { File = &file; }
        NOCOPY_NOMOVE(file_writer)

        bool stream_good() const noexcept override { return File && File->good(); }
        int stream_write(const void* data, int numBytes) noexcept override;
        void stream_flush() noexcept override;

        // does not support read operations
        int stream_read(void* dst, int max) noexcept override { (void)dst; (void)max; return 0; }
        int stream_peek(void* dst, int max) noexcept override { (void)dst; (void)max; return 0; }
        void stream_skip(int n) noexcept override { (void)n; }
    };

    /**
     * A generic binary file reader. This is not the best for small inputs, but excels with huge contiguous streams
     */
    class RPPAPI file_reader : public binary_stream, protected stream_source
    {
        rpp::file* File = nullptr;
    public:
        file_reader() noexcept : binary_stream{this} {}
        explicit file_reader(rpp::file& file)      noexcept : binary_stream{this}, File(&file) {}
        file_reader(rpp::file& file, int capacity) noexcept : binary_stream{capacity, this}, File(&file) {}
        ~file_reader() noexcept;
        void set_file(rpp::file& file) noexcept { File = &file; }
        NOCOPY_NOMOVE(file_reader)

        bool stream_good() const noexcept override { return File && File->good(); }

        // does not support write operations
        int stream_write(const void* data, int numBytes) noexcept override { (void)data; (void)numBytes; return 0; }

        void stream_flush() noexcept override;
        int stream_read(void* dst, int max) noexcept override;
        int stream_peek(void* dst, int max) noexcept override;
        void stream_skip(int n) noexcept override;
    };

#endif

    ////////////////////////////////////////////////////////////////////////////
} // namespace ReCpp
