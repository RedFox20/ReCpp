#pragma once
#include "strview.h"
#ifndef RPP_BINARY_READWRITE_NO_SOCKETS
#include "sockets.h"
#endif
#ifndef RPP_BINARY_READWRITE_NO_FILE_IO
#include "file_io.h"
#endif

namespace rpp /* ReCpp */ 
{
    ////////////////////////////////////////////////////////////////////////////

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
        #define MOVE(Class,value) Class(Class&&)=value;       Class&operator=(Class&&)=value;
        #define NOCOPY(Class)     Class(const Class&)=delete; Class&operator=(const Class&)=delete;
        #define NOCOPY_MOVE(Class)   MOVE(Class,default) NOCOPY(Class) 
        #define NOCOPY_NOMOVE(Class) MOVE(Class,delete)  NOCOPY(Class) 
    #endif

    #ifndef RPP_MINMAX_DEFINED
    #define RPP_MINMAX_DEFINED
        template<class T> T max(T a, T b) { return a > b ? a : b; }
        template<class T> T min(T a, T b) { return a < b ? a : b; }
        template<class T> T min3(T a, T b, T c) { return a < b ? (a<c?a:c) : (b<c?b:c); }
    #endif
    
    ////////////////////////////////////////////////////////////////////////////

    struct stream_buffer
    {
        static constexpr int MAX = 1024;

    protected:
        int Pos = 0;   // position in the buffer
        int Rem = 0;   // remaining bytes that can be read or written
        int Cap = MAX; // current buffer capacity
        char* Ptr;     // pointer to current buffer, either this->Buf or a dynamically allocated one
        char  Buf[MAX];

        stream_buffer() noexcept;
        explicit stream_buffer(int capacity) noexcept;
        ~stream_buffer() noexcept;

        void reserve_buffer(int capacity) noexcept;
    };

    ////////////////////////////////////////////////////////////////////////////
    
    /**
     * @brief binary_writer interface
     */
    struct writer_base
    {
        virtual ~writer_base() noexcept = default;

       /**
        * @return TRUE if stream can be written to, FALSE if the stream is closed
        */
        virtual bool good() const noexcept = 0;

        /**
         * Writes a block of data to the underlying storage target or network device
         * @return Number of bytes actually written. Must be [numBytes] or <= 0 on failure
         */
        virtual int unbuffered_write(const void* data, uint numBytes) = 0;
    };

    ////////////////////////////////////////////////////////////////////////////

    /**
     * @brief A buffered socket writer class.
     */
    struct binary_writer : writer_base, stream_buffer
    {
        /**
         * Creates a binary writer with a default write buffer of size [binary_writer::MAX]
         */
        binary_writer() noexcept;

        /**
         * Create binary writer with buffer capacity. If capacity == 0, then no buffering is used.
         */
        explicit binary_writer(int capacity) noexcept;
        virtual ~binary_writer() noexcept;

        // binary_writer is pure virtual and shouldn't be copied or moved
        NOCOPY_NOMOVE(binary_writer)

        char*data() const { return (char*)Buf; }
        uint size() const { return Pos; }
        uint capacity()  const { return Cap; }
        uint available() const { return MAX - Pos; }
        void clear() { Pos = 0; }
        strview view() const { return { data(), (int)size() }; }

        /** 
         * Flushes the buffer and changes the size of write buffer
         * Setting this to 0 will disable buffering. Setting it to <= MAX has no effect
         */
        void reserve(int capacity);

        /** @brief Flush write buffer on this writer */
        void flush();

        /** @brief Writes raw data into the buffer */
        binary_writer& write(const void* data, uint numBytes);

    private:
        bool check_unbuffered(const void* data, uint numBytes);

    public:
        /** @brief Writes object of type T into the buffer */
        template<class T> binary_writer& write(const T& data)
        {
            if (check_unbuffered(&data, sizeof(T)))
            {
                *(T*)&Buf[Pos] = data;
                Pos += sizeof(T);
            }
            return *this;
        }

        /** @brief Appends data from other binary_writer to this one */
        binary_writer& write(const binary_writer& w) { return write(w.Buf, w.Pos); }

        binary_writer& write_byte(byte value)     { return write(value); } /** @brief Writes a 8-bit unsigned byte into the buffer */
        binary_writer& write_short(short value)   { return write(value); } /** @brief Writes a 16-bit signed short into the buffer */
        binary_writer& write_ushort(ushort value) { return write(value); } /** @brief Writes a 16-bit unsigned short into the buffer */
        binary_writer& write_int(int value)       { return write(value); } /** @brief Writes a 32-bit signed integer into the buffer */
        binary_writer& write_uint(uint value)     { return write(value); } /** @brief Writes a 32-bit unsigned integer into the buffer */
        binary_writer& write_int64(int64 value)   { return write(value); } /** @brief Writes a 64-bit signed integer into the buffer */
        binary_writer& write_uint64(uint64 value) { return write(value); } /** @brief Writes a 64-bit unsigned integer into the buffer */

        /** @brief Write a length specified string to the buffer in the form of [uint16 len][data] */
        template<class Char> binary_writer& write_nstr(const Char* str, int len) {
            return write_ushort(ushort(len)).write((void*)str, len * sizeof(Char)); 
        }
        binary_writer& write(const strview& str)      { return write_nstr(str.str, str.len); }
        binary_writer& write(const std::string& str)  { return write_nstr(str.c_str(), (int)str.length()); }
        binary_writer& write(const std::wstring& str) { return write_nstr(str.c_str(), (int)str.length()); }
    };
    // explicit operator<< overloads
    inline binary_writer& operator<<(binary_writer& w, const strview& v)      { return w.write(v); }
    inline binary_writer& operator<<(binary_writer& w, const std::string& v)  { return w.write(v); }
    inline binary_writer& operator<<(binary_writer& w, const std::wstring& v) { return w.write(v); }
    inline binary_writer& operator<<(binary_writer& w, bool v)   { return w.write(v); }
    inline binary_writer& operator<<(binary_writer& w, char v)   { return w.write(v); }
    inline binary_writer& operator<<(binary_writer& w, byte v)   { return w.write(v); }
    inline binary_writer& operator<<(binary_writer& w, short v)  { return w.write(v); }
    inline binary_writer& operator<<(binary_writer& w, ushort v) { return w.write(v); }
    inline binary_writer& operator<<(binary_writer& w, int v)    { return w.write(v); }
    inline binary_writer& operator<<(binary_writer& w, uint v)   { return w.write(v); }
    inline binary_writer& operator<<(binary_writer& w, int64 v)  { return w.write(v); }
    inline binary_writer& operator<<(binary_writer& w, uint64 v) { return w.write(v); }
    inline binary_writer& operator<<(binary_writer& w, float v)  { return w.write(v); }
    inline binary_writer& operator<<(binary_writer& w, double v) { return w.write(v); }
    inline binary_writer& operator<<(binary_writer& w, binary_writer& m(binary_writer&)) { return m(w); }
    inline binary_writer& endl(binary_writer& w) { w.flush(); return w; }

    
    ////////////////////////////////////////////////////////////////////////////

    /**
     * @brief binary_reader interface
     */
    struct reader_base
    {
        virtual ~reader_base() noexcept = default;

        /**
         * @return TRUE if stream is open for reading, FALSE if it's closed
         */
        virtual bool good() const noexcept = 0;

        /**
         * @return Number of bytes available for reading; 0 if no bytes available; -1 if stream failed/closed
         */
        virtual int stream_available() const noexcept = 0;

        /**
         * @brief Reads bytes directly from the underlying stream
         * @param dst Destination buffer to read into
         * @param max Maximum number of characters to peek
         * @return Number of bytes actually read; 0 if nothing was read (no data); -1 if stream failed/closed
         */
        virtual int stream_read(void* dst, int max) noexcept = 0;

        /**
         * Peeks the stream for the next few bytes. Not all streams can be peeked, so this implementation is optional.
         * @param dst Destination buffer to read into
         * @param max Maximum number of characters to peek
         * @return Number of bytes actually peeked. Not all streams can be peeked, in which case result is 0;
         */
        virtual int stream_peek(void* dst, int max) noexcept { (void)dst; (void)max; return 0; }

        /**
         * Tries to flush the underlying read stream. Not all read streams can be flushed, so this is optional.
         */
        virtual void stream_flush() noexcept {}

        /**
         * Skips a specific number of bytes
         */
        virtual void stream_skip(int n) noexcept = 0;
    };

    ////////////////////////////////////////////////////////////////////////////

    /**
     * @brief A buffered socket reader class.
     */
    struct binary_reader : reader_base, stream_buffer
    {
        /**
         * Creates a new binary_reader with default buffer capacity of [binary_reader::MAX]
         */
        binary_reader() noexcept;
        /**
         * Creates a new binary_reader with given capacity buffer. If set to 0, then buffering is disabled
         */
        explicit binary_reader(int capacity) noexcept;
        virtual ~binary_reader() noexcept;

        // binary_reader is pure virtual and shouldn't be copied or moved
        NOCOPY_NOMOVE(binary_reader)

        const char* data() const { return &Ptr[Pos]; }
        uint pos()       const { return Pos; }
        /* total bytes read from the underlying buffer */
        uint size()      const { return Pos + Rem; }
        uint available() const { return Rem + stream_available(); }
        strview view()   const { return { data(), (int)size() }; }

        /** 
         * Flushes the buffer and changes the size of write buffer
         * Setting this to 0 will disable buffering. Setting it to <= MAX has no effect
         */
        void reserve(int capacity);

        /* flushes both buffered data and any storage buffering */
        void flush();

    protected:
        void buf_fill();
        uint buf_read(void* dst, uint cnt);
        uint _read(void* dst, uint cnt, uint bufn);
        template<class T> uint buf_read(T& dst)
        {
            dst = *(T*)&Buf[Pos];
            Pos += sizeof(T);
            Rem -= sizeof(T);
            return sizeof(T);
        }

    public:
        uint read(void* dst, uint cnt);
        template<class T> uint read(T& dst)
        {
            uint n = Rem;
            if (n >= sizeof(T)) return buf_read(dst);  // best case, all from buffer
            return _read(&dst, sizeof(T), n);          // fallback partial read
        }

        uint peek(void* dst, uint cnt);
        template<class T> uint peek(T& dst)
        {
            if (Rem <= 0) buf_fill(); // fill before peek if possible
            dst = *(T*)&Buf[Pos];
            return sizeof(T);
        }
        void skip(uint n);
        void undo(uint n);

        /** @brief Reads generic POD type data and returns it */
        template<class T> T read() { T out; read(out); return out; }
        /** @brief Peeks generic POD type data and returns it without advancing the streampos */
        template<class T> T peek() { T out; peek(out); return out; }

        byte   read_byte()   { return read<byte>();  } /** @brief Reads a 8-bit unsigned byte   */
        short  read_short()  { return read<short>(); } /** @brief Reads a 16-bit signed short   */
        ushort read_ushort() { return read<ushort>();} /** @brief Reads a 16-bit unsigned short */
        int    read_int()    { return read<int>();   } /** @brief Reads a 32-bit signed integer */
        uint   read_uint()   { return read<uint>();  } /** @brief Reads a 32-bit unsigned integer */
        int64  read_int64()  { return read<int64>(); } /** @brief Reads a 64-bit signed integer   */
        uint64 read_uint64() { return read<uint64>();} /** @brief Reads a 64-bit unsigned integer */

        byte   peek_byte()   { return peek<byte>();  } /** @brief Peeks a 8-bit unsigned byte without advancing the streampos   */
        short  peek_short()  { return peek<short>(); } /** @brief Peeks a 16-bit signed short without advancing the streampos   */
        ushort peek_ushort() { return peek<ushort>();} /** @brief Peeks a 16-bit unsigned short without advancing the streampos */
        int    peek_int()    { return peek<int>();   } /** @brief Peeks a 32-bit signed integer without advancing the streampos */
        uint   peek_uint()   { return peek<uint>();  } /** @brief Peeks a 32-bit unsigned integer without advancing the streampos */
        int64  peek_int64()  { return peek<int64>(); } /** @brief Peeks a 64-bit signed integer without advancing the streampos   */
        uint64 peek_uint64() { return peek<uint64>();} /** @brief Peeks a 64-bit unsigned integer without advancing the streampos */


        /** @brief Reads a length specified string to the std::string in the form of [uint16 len][data] */
        template<class Char> binary_reader& read(std::basic_string<Char>& str) {
            uint n = min<uint>(read_ushort(), available());
            str.resize(n);
            read((void*)str.data(), sizeof(Char) * n);
            return *this;
        }
        /** @brief Reads a length specified string to the dst buffer in the form of [uint16 len][data] and returns actual length */
        template<class Char> uint read_nstr(Char* dst, uint maxLen) {
            uint n = min3<uint>(read_ushort(), available(), maxLen);
            read((void*)dst, sizeof(Char) * n);
            return n;
        }
        /** @brief Peeks a length specified string to the std::string in the form of [uint16 len][data] */
        template<class Char> binary_reader& peek(std::basic_string<Char>& str) {
            uint n = min<uint>(read_ushort(), available());
            str.resize(n);
            peek((void*)str.data(), sizeof(Char) * n);
            undo(2); // undo read_ushort
            return *this;
        }
        /** @brief Peeks a length specified string to the dst buffer in the form of [uint16 len][data] and returns actual length */
        template<class Char> uint peek_nstr(Char* dst, uint maxLen) {
            uint n = min3<uint>(read_ushort(), available(), maxLen);
            n = peek((void*)dst, sizeof(Char) * n) / sizeof(Char);
            undo(2); // undo read_ushort
            return n;
        }

        std::string  read_string()  { std::string  s; read(s); return s; } /** @brief Reads a length specified [uint16 len][data] string */
        std::wstring read_wstring() { std::wstring s; read(s); return s; } /** @brief Reads a length specified [uint16 len][data] wstring */
        std::string  peek_string()  { std::string  s; peek(s); return s; } /** @brief Peeks a length specified [uint16 len][data] string */
        std::wstring peek_wstring() { std::wstring s; peek(s); return s; } /** @brief Peeks a length specified [uint16 len][data] wstring */
        strview      peek_strview() { strview s; peek(s); return s; }      /** @brief Peeks a length specified [uint16 len][data] string view */
    };
    // explicit operator>> overloads
    inline binary_reader& operator>>(binary_reader& r, strview& v)      { r.peek(v); return r; }
    inline binary_reader& operator>>(binary_reader& r, std::string& v)  { r.read(v); return r; }
    inline binary_reader& operator>>(binary_reader& r, std::wstring& v) { r.read(v); return r; }
    inline binary_reader& operator>>(binary_reader& r, bool& v)   { r.read(v); return r; }
    inline binary_reader& operator>>(binary_reader& r, char& v)   { r.read(v); return r; }
    inline binary_reader& operator>>(binary_reader& r, byte& v)   { r.read(v); return r; }
    inline binary_reader& operator>>(binary_reader& r, short& v)  { r.read(v); return r; }
    inline binary_reader& operator>>(binary_reader& r, ushort& v) { r.read(v); return r; }
    inline binary_reader& operator>>(binary_reader& r, int& v)    { r.read(v); return r; }
    inline binary_reader& operator>>(binary_reader& r, uint& v)   { r.read(v); return r; }
    inline binary_reader& operator>>(binary_reader& r, int64& v)  { r.read(v); return r; }
    inline binary_reader& operator>>(binary_reader& r, uint64& v) { r.read(v); return r; }
    inline binary_reader& operator>>(binary_reader& r, float& v)  { r.read(v); return r; }
    inline binary_reader& operator>>(binary_reader& r, double& v) { r.read(v); return r; }
    inline binary_reader& operator>>(binary_reader& r, binary_reader& m(binary_reader&)) { return m(r); }


    ////////////////////////////////////////////////////////////////////////////


#ifndef RPP_BINARY_READWRITE_NO_SOCKETS

    template<class binary_base> struct binary_socket_stream : binary_base
    {
        rpp::socket* Sock = nullptr;

        binary_socket_stream() noexcept {}
        explicit binary_socket_stream(rpp::socket& sock) noexcept : Sock(&sock) {}
        binary_socket_stream(rpp::socket& sock, int capacity) noexcept : binary_base(capacity), Sock(&sock) {}

        bool good() const noexcept override { return Sock && Sock->good(); }
    };

    /**
     * A generic binary socket writer. For UDP sockets, use socket::bind to set the destination ipaddress.
     */
    struct socket_writer : binary_socket_stream<binary_writer>
    {
        int unbuffered_write(const void* data, uint numBytes) override;
    };

    /**
     * A generic binary socket reader. For UDP sockets, use socket::bind to set the destination ipaddress.
     */
    struct socket_reader : binary_socket_stream<binary_reader>
    {
        int stream_available() const noexcept override;
        int stream_read(void* dst, int max) noexcept override;
        int stream_peek(void* dst, int max) noexcept override;
        void stream_flush() noexcept override;
        void stream_skip(int n) noexcept override;
    };

#endif

    ////////////////////////////////////////////////////////////////////////////

#ifndef RPP_BINARY_READWRITE_NO_FILE_IO

    template<class binary_base> struct binary_file_stream : binary_base
    {
        rpp::file* File = nullptr;

        binary_file_stream() noexcept {}
        explicit binary_file_stream(rpp::file& file) noexcept : File(&file) {}
        binary_file_stream(rpp::file& file, int capacity) noexcept : binary_base(capacity), File(&file) {}

        bool good() const noexcept override { return File && File->good(); }
    };

    /**
     * A generic binary file writer.
     */
    struct file_writer : binary_file_stream<binary_writer>
    {
        int unbuffered_write(const void* data, uint numBytes) override;
    };

    /**
     * A generic binary file reader.
     */
    struct file_reader : binary_file_stream<binary_reader>
    {
        int stream_available() const noexcept override;
        int stream_read(void* dst, int max) noexcept override;
        int stream_peek(void* dst, int max) noexcept override;
        void stream_flush() noexcept override;
        void stream_skip(int n) noexcept override;
    };

#endif

    ////////////////////////////////////////////////////////////////////////////

    struct binary_buffer_writer : binary_writer
    {
        int unbuffered_write(const void* data, uint numBytes) override;
    };

    ////////////////////////////////////////////////////////////////////////////
} // namespace ReCpp