#pragma once
/**
 * Efficient binary streams, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "strview.h"
#include "minmax.h"
#ifndef RPP_BINARY_READWRITE_NO_SOCKETS
#  include "sockets.h"
#endif
#ifndef RPP_BINARY_READWRITE_NO_FILE_IO
#  include "file_io.h"
#endif

#if _MSC_VER
#  pragma warning(disable: 4251) // class 'std::*' needs to have dll-interface to be used by clients of struct 'rpp::*'
#endif

namespace rpp
{
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
        virtual int stream_write(const void* data, int numBytes) noexcept = 0;

        /**
         * Flush all read/write buffers on the stream
         */
        virtual void stream_flush() noexcept = 0;

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

        void disable_buffering() noexcept;

        FINLINE const char* data()  const noexcept { return &Ptr[ReadPos]; }
        FINLINE char*       data()        noexcept { return &Ptr[ReadPos]; }
        FINLINE const char* begin() const noexcept { return &Ptr[ReadPos]; }
        FINLINE const char* end()   const noexcept { return &Ptr[End]; }
        FINLINE int readpos()      const noexcept { return ReadPos; }
        FINLINE int writepos()     const noexcept { return WritePos; }
        // @return Number of bytes in the read buffer
        FINLINE int size()      const noexcept { return End - ReadPos; }
        FINLINE int capacity()  const noexcept { return Cap; }
        FINLINE strview view()  const noexcept { return { data(), (int)size() }; }

        // @return Total buffered bytes and available stream bytes: 
        //         size() + stream_available()
        int available() const noexcept;

        /**
         * Sets the buffer position and size to 0; no data is flushed
         */
        void clear() noexcept;

        /**
         * Rewinds the Read/Write head to a specific position in the BUFFER.
         * All subsequent read/write operations will begin from the given position.
         * @note New pos is range checked and clamped
         */
        void rewind(int pos = 0) noexcept;

        /**
         * @return TRUE if this stream is open and data is available
         */
        bool good() const noexcept;
        FINLINE explicit operator bool() const noexcept { return good(); }

        /** 
         * Flushes the buffer and changes the size of write buffer
         * Setting this to 0 will disable buffering. Setting it to <= binary_stream::SBSize has no effect
         */
        void reserve(int capacity) noexcept;

        /** 
         * @brief Flush write buffer and then flush to the underlying stream.
         */
        void flush() noexcept;

        /**
         * @brief Flush write buffer only. Underlying stream will take care of its own things.
         */
        void flush_write_buffer() noexcept;

    private:
        NOINLINE void ensure_space(int numBytes) noexcept;

    public:
        ////////////////////// ----- Writer Fields ----- //////////////////////////

        /** @brief Writes raw data into the buffer */
        binary_stream& write(const void* data, int numBytes) noexcept;

        template<class T>
        static constexpr bool is_trivial_type = std::is_default_constructible<T>::value
                                             && std::is_trivially_destructible<T>::value;

        /** @brief Writes object of type T into the buffer */
        template<class T, typename = std::enable_if_t<is_trivial_type<T>>> 
        binary_stream& write(const T& data) noexcept
        {
            ensure_space((int)sizeof(T));
            unsafe_write(data);
            return *this;
        }

    private:
        void unsafe_write(const void* data, int numBytes) noexcept;

        void unsafe_write(rpp::uint16 data) noexcept;
        void unsafe_write(rpp::uint32 data) noexcept;
        void unsafe_write(rpp::uint64 data) noexcept;
        FINLINE void unsafe_write(rpp::int16 data) noexcept { unsafe_write((rpp::uint16)data); }
        FINLINE void unsafe_write(rpp::int32 data) noexcept { unsafe_write((rpp::uint32)data); }
        FINLINE void unsafe_write(rpp::int64 data) noexcept { unsafe_write((rpp::uint64)data); }

        void unsafe_write(float data) noexcept;
        void unsafe_write(double data) noexcept;

        template<class T>
        void unsafe_write(const T& data) noexcept
        {
            *reinterpret_cast<T*>(Ptr + WritePos) = data;
            WritePos += (int)sizeof(T);
            End      += (int)sizeof(T);
        }

    public:

        /** @brief Appends data from other binary_writer to this one */
        FINLINE binary_stream& write(const binary_stream& w) noexcept { return write(w.data(), w.size()); }
        /** @brief Writes a 8-bit unsigned byte into the buffer */
        FINLINE binary_stream& write_byte(rpp::byte value)  noexcept { return write(value); }
        /** @brief Writes a 16-bit signed int into the buffer */
        FINLINE binary_stream& write_int16(rpp::int16 value)   noexcept { return write(value); }
        /** @brief Writes a 16-bit unsigned int into the buffer */
        FINLINE binary_stream& write_uint16(rpp::uint16 value) noexcept { return write(value); }
        /** @brief Writes a 32-bit signed integer into the buffer */
        FINLINE binary_stream& write_int32(rpp::int32 value)       noexcept { return write(value); }
        /** @brief Writes a 32-bit unsigned integer into the buffer */
        FINLINE binary_stream& write_uint32(rpp::uint32 value)     noexcept { return write(value); }
        /** @brief Writes a 64-bit signed integer into the buffer */
        FINLINE binary_stream& write_int64(rpp::int64 value)   noexcept { return write(value); }
        /** @brief Writes a 64-bit unsigned integer into the buffer */
        FINLINE binary_stream& write_uint64(rpp::uint64 value) noexcept { return write(value); }
        /** @brief Writes a 32-bit float into the buffer */
        FINLINE binary_stream& write_float(float value)   noexcept { return write(value); }
        /** @brief Writes a 64-bit double into the buffer */
        FINLINE binary_stream& write_double(double value) noexcept { return write(value); }


        using strlen_t = rpp::uint16;

        /** @brief Write a length specified string to the buffer in the form of [strlen_t len][data] */
        template<class Char> binary_stream& write_nstr(const Char* str, int len) noexcept {
            return write<strlen_t>(len).write((void*)str, len * (int)sizeof(Char));
        }
        FINLINE binary_stream& write(const strview& str)      noexcept { return write_nstr(str.str, str.len); }
        FINLINE binary_stream& write(const std::string& str)  noexcept { return write_nstr(str.c_str(), (int)str.length()); }
        FINLINE binary_stream& write(const std::wstring& str) noexcept { return write_nstr(str.c_str(), (int)str.length()); }

        template<class T, class A>
        binary_stream& write(const std::vector<T, A>& v) noexcept
        {
            rpp::int32 n = (int)v.size();
            int cap = min(4 + n * (int)sizeof(T), 1024 * 1024); // fuzzy capacity
            ensure_space(cap);

            RPP_CXX17_IF_CONSTEXPR(is_trivial_type<T>)
            {
                unsafe_write<rpp::int32>(n);
                unsafe_write(v.data(), n * (int)sizeof(T));
            }
            else
            {
                write<rpp::int32>(n);
                for (const T& item : v)
                    *this << item; // @note ADL fails easily here, use write_vector with custom writer instead
            }
            return *this;
        }

        template<class T, class A, class Writer>
        binary_stream& write(const std::vector<T, A>& v, const Writer& writer) noexcept
        {
            rpp::int32 n = (int)v.size();
            int cap = min(4 + n * (int)sizeof(T), 1024*1024); // fuzzy capacity
            ensure_space(cap);

            write<rpp::int32>(n);
            for (const T& item : v)
                writer(*this, item);
            return *this;
        }

        ////////////////////// ----- Reader Fields ----- //////////////////////////

    private:
        int unsafe_buffer_fill() noexcept;
        int unsafe_buffer_read(void* dst, int bytesToRead) noexcept;
        int fragmented_read(void* dst, int bytesToRead) noexcept;

        template<class T>
        int unsafe_buffer_read(T& dst) noexcept
        {
            unsafe_buffer_decode(dst);
            ReadPos += (int)sizeof(T);
            return (int)sizeof(T);
        }

        template<class T>
        void unsafe_buffer_decode(T& dst) noexcept
        {
            dst = *reinterpret_cast<T*>(Ptr + ReadPos);
        }

        void unsafe_buffer_decode(rpp::uint16& dst) noexcept;
        void unsafe_buffer_decode(rpp::uint32& dst) noexcept;
        void unsafe_buffer_decode(rpp::uint64& dst) noexcept;
        FINLINE void unsafe_buffer_decode(rpp::int16& dst) noexcept { unsafe_buffer_decode((rpp::uint16&)dst); }
        FINLINE void unsafe_buffer_decode(rpp::int32& dst) noexcept { unsafe_buffer_decode((rpp::uint32&)dst); }
        FINLINE void unsafe_buffer_decode(rpp::int64& dst) noexcept { unsafe_buffer_decode((rpp::uint64&)dst); }
        void unsafe_buffer_decode(float& dst) noexcept;
        void unsafe_buffer_decode(double& dst) noexcept;

    public:
        int read(void* dst, int bytesToRead) noexcept;
        template<class T> int read(T& dst) noexcept
        {
            if (size() >= (int)sizeof(T))
                return unsafe_buffer_read(dst); // best case, all from buffer
            return fragmented_read(&dst, (int)sizeof(T)); // fallback partial read
        }

    private:
        int peek_fetch_avail() noexcept;

    public:
        int peek(void* dst, int bytesToPeek) noexcept;

        FINLINE int peek(rpp::uint16& dst) noexcept { return peek((rpp::int16&)dst); }

        template<class T>
        int peek(T& dst) noexcept
        {
            int avail = peek_fetch_avail();
            if (avail < (int)sizeof(T)) return 0;
            unsafe_buffer_decode(dst);
            return (int)sizeof(T);
        }

        /** @brief Skips data in the buffer and in the stream */
        void skip(int n) noexcept;

        /** @brief Attempts to undo a previous read from the buffer. Not very reliable. */
        void undo(int n) noexcept;

        /** @brief Reads generic POD type data and returns it */
        template<class T> FINLINE T read() noexcept { T out{}; read(out); return out; }
        /** @brief Peeks generic POD type data and returns it without advancing the streampos */
        template<class T> FINLINE T peek() noexcept { T out{}; peek(out); return out; }

        /** @brief Reads a 8-bit unsigned byte   */
        FINLINE rpp::byte read_byte() noexcept { return read<uint8_t>(); }
        /** @brief Reads a 16-bit signed int   */
        FINLINE rpp::int16 read_int16() noexcept { return read<rpp::int16>(); }
        /** @brief Reads a 16-bit unsigned int */
        FINLINE rpp::uint16 read_uint16() noexcept { return read<rpp::uint16>(); }
        /** @brief Reads a 32-bit signed integer */
        FINLINE rpp::int32 read_int32() noexcept { return read<rpp::int32>(); }
        /** @brief Reads a 32-bit unsigned integer */
        FINLINE rpp::uint32 read_uint32() noexcept { return read<rpp::uint32>(); }
        /** @brief Reads a 64-bit signed integer   */
        FINLINE rpp::int64 read_int64() noexcept { return read<rpp::int64>(); }
        /** @brief Reads a 64-bit unsigned integer */
        FINLINE rpp::uint64 read_uint64() noexcept { return read<rpp::uint64>(); }
        /** @brief Reads a 32-bit float */
        FINLINE float read_float() noexcept { return read<float>();  }
        /** @brief Reads a 64-bit float */
        FINLINE double read_double() noexcept { return read<double>(); }

        /** @brief Peeks a 8-bit unsigned byte without advancing the streampos   */
        FINLINE rpp::byte peek_byte() noexcept { return peek<rpp::byte>();}
        /** @brief Peeks a 16-bit signed int without advancing the streampos   */
        FINLINE rpp::int16 peek_int16() noexcept { return peek<rpp::int16>(); }
        /** @brief Peeks a 16-bit unsigned int without advancing the streampos */
        FINLINE rpp::uint16 peek_uint16() noexcept { return peek<rpp::uint16>(); }
        /** @brief Peeks a 32-bit signed integer without advancing the streampos */
        FINLINE rpp::int32 peek_int32() noexcept { return peek<rpp::int32>(); }
        /** @brief Peeks a 32-bit unsigned integer without advancing the streampos */
        FINLINE rpp::uint32 peek_uint32() noexcept { return peek<rpp::uint32>(); }
        /** @brief Peeks a 64-bit signed integer without advancing the streampos   */
        FINLINE rpp::int64 peek_int64() noexcept { return peek<rpp::int64>(); }
        /** @brief Peeks a 64-bit unsigned integer without advancing the streampos */
        FINLINE rpp::uint64 peek_uint64() noexcept { return peek<rpp::uint64>(); }
        /** @brief Peeks a 32-bit float */
        FINLINE float peek_float() noexcept { return peek<float>();  }
        /** @brief Peeks a 64-bit float */
        FINLINE double peek_double() noexcept { return peek<double>(); }

        /** @brief Reads a length specified string to the std::string in the form of [strlen_t len][data] */
        template<class Char> binary_stream& read(std::basic_string<Char>& str) noexcept {
            strlen_t n = read<strlen_t>();
            str.resize(size_t(n));
            read((void*)str.data(), (int)sizeof(Char) * n);
            return *this;
        }
        /** @brief Reads a length specified string to the dst buffer in the form of [strlen_t len][data] and returns actual length */
        template<class Char> int read_nstr(Char* dst, int maxLen) noexcept {
            strlen_t n = read<strlen_t>();
            strlen_t m = min<strlen_t>(n, maxLen);
            int actual = read((void*)dst, (int)sizeof(Char) * m);
            if (n > actual) // we must skip over any unread bytes to keep stream consistency
                skip(n - actual);
            return actual;
        }
        /** @brief Peeks a length specified string to the std::string in the form of [strlen_t len][data] */
        template<class Char> binary_stream& peek(std::basic_string<Char>& str) noexcept {
            // we cannot peek, because read<T>() causes side effects and read buffer is too small
            if (size() < (int)sizeof(strlen_t)) {
                str.clear();
                return *this;
            }
            strlen_t n = read<strlen_t>(); // @warning Buffer change side effect
            n = min<int>(n, size());
            str.resize(size_t(n));
            peek((void*)str.data(), (int)sizeof(Char) * n);
            undo(sizeof(strlen_t)); // undo read_int
            return *this;
        }
        /** @brief Peeks a length specified string to the dst buffer in the form of [strlen_t len][data] and returns actual length */
        template<class Char> int peek_nstr(Char* dst, int maxLen) noexcept {
            // we cannot peek, because read<T>() causes side effects and read buffer is too small
            if (size() < (int)sizeof(strlen_t)) {
                dst[0] = Char('\0');
                return 0;
            }
            strlen_t n = read<strlen_t>();  // @warning Buffer change side effect
            n = min3<int>(n, size(), maxLen);
            n = peek((void*)dst, (int)sizeof(Char) * n) / (int)sizeof(Char);
            undo(sizeof(strlen_t)); // undo read_int
            return n;
        }

        /** @brief Reads a length specified [strlen_t len][data] string */
        FINLINE std::string read_string() noexcept { std::string  s; read(s); return s; }
        /** @brief Reads a length specified [strlen_t len][data] wstring */
        FINLINE std::wstring read_wstring() noexcept { std::wstring s; read(s); return s; }
        /** @brief Peeks a length specified [strlen_t len][data] string */
        FINLINE std::string peek_string() noexcept { std::string  s; peek(s); return s; }
        /** @brief Peeks a length specified [strlen_t len][data] wstring */
        FINLINE std::wstring peek_wstring() noexcept { std::wstring s; peek(s); return s; }
        /** @brief Peeks a length specified [strlen_t len][data] string view */
        strview peek_strview() noexcept;
    
        template<class T, class A>
        binary_stream& read(std::vector<T, A>& out) noexcept
        {
            int n = read_int32();
            out.clear();
            RPP_CXX17_IF_CONSTEXPR(is_trivial_type<T>)
            {
                out.resize(size_t(n));
                read(out.data(), n * (int)sizeof(T));
            }
            else
            {
                out.reserve(size_t(n));
                for (int i = 0; i < n; ++i) {
                    *this >> out.emplace_back();
                }
            }
            return *this;
        }

        template<class T, class A, class Reader>
        binary_stream& read(std::vector<T, A>& out, const Reader& reader) noexcept
        {
            int n = read_int32();
            out.clear();
            out.reserve(size_t(n));
            for (int i = 0; i < n; ++i)
                reader(*this, out.emplace_back());
            return *this;
        }
    };

    // explicit operator<< overloads
    inline binary_stream& operator<<(binary_stream& w, const strview& v)      noexcept { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, const std::string& v)  noexcept { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, const std::wstring& v) noexcept { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, bool v)   noexcept { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, char v)   noexcept { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, rpp::byte v)   noexcept { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, rpp::int16 v)  noexcept { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, rpp::uint16 v) noexcept { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, rpp::int32 v)  noexcept { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, rpp::uint32 v) noexcept { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, rpp::int64 v)  noexcept { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, rpp::uint64 v) noexcept { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, float v)  noexcept { return w.write(v); }
    inline binary_stream& operator<<(binary_stream& w, double v) noexcept { return w.write(v); }
    template<class T>
    inline binary_stream& operator<<(binary_stream& w, const std::vector<T>& v) noexcept
    {
        return w.write(v);
    }
    inline binary_stream& operator<<(binary_stream& w, binary_stream& m(binary_stream&)) noexcept { return m(w); }
    inline binary_stream& endl(binary_stream& w) noexcept { w.flush(); return w; }

    inline binary_stream& operator>>(binary_stream& r, strview& v)      noexcept { r.peek(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, std::string& v)  noexcept { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, std::wstring& v) noexcept { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, bool& v)   noexcept { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, char& v)   noexcept { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, rpp::byte& v)   noexcept { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, rpp::int16& v)  noexcept { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, rpp::uint16& v) noexcept { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, rpp::int32& v)  noexcept { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, rpp::uint32& v) noexcept { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, rpp::int64& v)  noexcept { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, rpp::uint64& v) noexcept { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, float& v)  noexcept { r.read(v); return r; }
    inline binary_stream& operator>>(binary_stream& r, double& v) noexcept { r.read(v); return r; }
    template<class T, class A>
    inline binary_stream& operator>>(binary_stream& r, std::vector<T, A>& out) noexcept
    {
        return r.read(out);
    }
    inline binary_stream& operator>>(binary_stream& r, binary_stream& m(binary_stream&)) noexcept { return m(r); }



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

#endif // ifndef RPP_BINARY_READWRITE_NO_SOCKETS

    ////////////////////////////////////////////////////////////////////////////

#ifndef RPP_BINARY_READWRITE_NO_FILE_IO

    /**
     * A generic binary file writer. This is not the best for small inputs, but excels with huge contiguous streams
     */
    class RPPAPI file_writer : public binary_stream, protected stream_source
    {
        rpp::file Owned;
        rpp::file* File = nullptr;
    public:
        file_writer() noexcept : binary_stream{this} {}
        explicit file_writer(rpp::file& file)      noexcept : binary_stream{this},           File(&file) {}
        file_writer(rpp::file& file, int capacity) noexcept : binary_stream{capacity, this}, File(&file) {}

        /**
         * Valid flags: READWRITE, CREATENEW, APPEND
         */
        explicit file_writer(const std::string& pathToFile, file::mode mode = file::CREATENEW) noexcept // NOLINT
            : binary_stream{this}, Owned{pathToFile, mode}, File{&Owned} {}

        file_writer(const std::string& pathToFile, int capacity, file::mode mode = file::CREATENEW) noexcept // NOLINT
            : binary_stream{capacity, this}, Owned{pathToFile, mode}, File{&Owned} {}

        ~file_writer() noexcept;

        /** @return Tells the current virtual write position of the stream */
        int tell() const noexcept { return File->tell() + writepos(); }

        /** @return Currently flushed size of the file stream */
        int stream_size() const noexcept { return File->size(); }

        /**
         * Close the file stream. Any seek, write, etc. Operations are not valid after this call.
         * stream_good() and good() will return false
         * @warning This will flush the write buffer!
         * @note This will also clear the read buffer!
         */
        void close() noexcept
        {
            flush_write_buffer();
            clear();
            File->close();
        }

        /**
         * Flushes any write buffers and seeks to the given position in the stream
         * @return The new position in the stream
         * @note This will also clear the read buffer!
         */
        int seek(int filepos, int seekmode = 0) noexcept
        {
            flush_write_buffer();
            clear();
            return File->seek(filepos, seekmode);
        }

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
        rpp::file Owned;
        rpp::file* File = nullptr;
    public:
        file_reader() noexcept : binary_stream{this} {}
        explicit file_reader(rpp::file& file)      noexcept : binary_stream{this},           File(&file) {}
        file_reader(rpp::file& file, int capacity) noexcept : binary_stream{capacity, this}, File(&file) {}
        
        explicit file_reader(const std::string& pathToFile) noexcept // NOLINT
            : binary_stream{this}, Owned{pathToFile, file::READONLY}, File{&Owned} {}
        
        file_reader(const std::string& pathToFile, int capacity) noexcept // NOLINT
            : binary_stream{capacity, this}, Owned{pathToFile, file::READONLY}, File{&Owned} {}
        
        ~file_reader() noexcept;

        /** @return Tells the current virtual read position of the stream */
        int tell() const noexcept { return File->tell() - size(); }

        /** @return Currently flushed size of the file stream */
        int stream_size() const noexcept { return File->size(); }

        /**
         * Close the file stream. Any seek, read, etc. Operations are not valid after this call.
         * stream_good() and good() will return false
         * @note This will also clear the read buffer!
         */
        void close() noexcept
        {
            clear();
            File->close();
        }

        /**
         * Clears any read buffers and seeks to the given position in the stream
         * @return The new position in the stream
         * @note This will clear the read buffer!
         */
        int seek(int filepos, int seekmode = 0) noexcept
        {
            clear();
            return File->seek(filepos, seekmode);
        }

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

#endif // ifndef RPP_BINARY_READWRITE_NO_FILE_IO

    ////////////////////////////////////////////////////////////////////////////
} // namespace ReCpp
