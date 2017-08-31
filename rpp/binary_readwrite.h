#pragma once
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

    
    /**
     * binary_reader mixin adapter interface
     */
    struct reader_base
    {
        virtual ~reader_base() noexcept = default;
        virtual int read(void* dst, int cnt) noexcept = 0;
        virtual int peek(void* dst, int cnt) noexcept = 0;
        virtual void skip(int n) noexcept = 0;
        virtual void undo(int n) noexcept = 0;
    };

    /**
     * @brief A buffered socket writer class.
     */
    struct binary_writer
    {
        static const int MAX = 1024;
        int Pos; // size
        rpp::socket* Sock;
        char Buf[MAX];

        binary_writer();
        binary_writer(rpp::socket& s);
        ~binary_writer();

        char*data() const { return (char*)Buf; }
        uint size() const { return Pos; }
        uint available() const { return MAX - Pos; }
        void clear() { Pos = 0; }

        /** @brief Flush write buffer on this writer */
        void flush();
        /** @brief Writes raw data into the buffer */
        binary_writer& write(const void* data, uint numBytes);
        /** @brief Writes object of type T into the buffer */
        template<class T> binary_writer& write(const T& data) {
            if (uint(MAX - Pos) < sizeof(T))
                flush(); // forced flush
            *(T*)&Buf[Pos] = data, Pos += sizeof(T);
            return *this;
        }
        /** @brief Appends data from other binary_writer to this one */
        binary_writer& write(const binary_writer& w) {
            return write(w.Buf, w.Pos);
        }

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



    /**
     * @brief A buffered socket reader class.
     */
    struct binary_reader
    {
        static const int MAX = 1024;
        int Pos; // buf pos
        int Rem; // buf rem
        rpp::socket* Sock;
        char Buf[MAX];

        binary_reader();
        binary_reader(rpp::socket& s);

        uint pos()       const { return Pos; }
        uint size()      const { return Pos + Rem; } /* network stream total bytes read */
        uint available() const { return Rem + Sock->available(); }

        /* flushes both buffered data and any storage buffering */
        void flush();
        void buf_fill();
        uint buf_read(void* dst, uint cnt);
        uint read(void* dst, uint cnt);
        uint _read(void* dst, uint cnt, uint bufn);
        uint peek(void* dst, uint cnt);
        template<class T> uint buf_read(T& dst)  {
            dst = *(T*)&Buf[Pos];
            Pos += sizeof(T), Rem -= sizeof(T); 
            return sizeof(T);
        }
        template<class T> uint read(T& dst) {
            uint n = Rem;
            if (n >= sizeof(T)) return buf_read(dst);  // best case, all from buffer
            return _read(&dst, sizeof(T), n);                 // fallback partial read
        }
        template<class T> uint peek(T& dst) {
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

} // namespace ReCpp