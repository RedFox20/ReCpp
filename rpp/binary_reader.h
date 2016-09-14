#pragma once
#ifndef RPP_BINARY_READER_H
#define RPP_BINARY_READER_H
#include <string>
#include <vector>
#include <cstdlib>    // malloc/free
#include <cstdio>     // fopen, fileno
#include <sys/stat.h> // fstat
#include "strview.h"
#include "sockets.h"  // rpp::socket

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
		virtual uint read(void* dst, uint cnt) = 0;
		virtual uint peek(void* dst, uint cnt) = 0;
		virtual void skip(uint n) = 0;
		virtual void undo(uint n) = 0;
	};

/**
 * A generic data reader.
 * Implementation is defined by container template parameter.
 * Container class must properly implement its copy/move constructor rules
 * Read source container implemention must define the following functions:
 *
 *     uint max();
 *     uint pos();
 *     uint size();
 *     uint available();
 *     void flush();
 *
 *     uint read(void* dst, uint cnt);
 *     uint peek(void* dst, uint cnt);
 *     template<class T> uint read(T& dst);
 *     template<class T> uint peek(T& dst);
 *     void skip(uint n);
 *     void undo(uint n);
 */
template<class read_impl> struct binary_reader : public reader_base, public read_impl
{
	using read_impl::read_impl; // inherit base constructors

	/** @brief Reads numbytes of raw data into dst buffer */
	uint read(void* dst, uint numBytes) override { return read_impl::read(dst, numBytes); }
	/** @brief Peeks numbytes of raw data into dst buffer without advancing the streampos */
	uint peek(void* dst, uint numBytes) override { return read_impl::peek(dst, numBytes); }

	/** @brief Reads generic POD type data into dst */
	template<class T> uint read(T& dst) {
		uint n = read_impl::read(dst);
		if (!n) dst = T(); // read failed, default construct
		return n;
	}
	/** @brief Peeks a generic POD type data without advancing the streampos */
	template<class T> uint peek(T& dst) {
		uint n = read_impl::peek(dst);
		if (!n) dst = T(); // read failed, default construct
		return n;
	}

	/** @brief Reads generic POD type data and returns it */
	template<class T> T read() { T out; read(out); return out; }
	/** @brief Peeks generic POD type data and returns it without advancing the streampos */
	template<class T> T peek() { T out; peek(out); return out; }
	/** @brief Advances streampos by discarding bytes */
	void skip(uint n) override { read_impl::skip(n); }
	/** @brief Can be used to undo read operations, but won't work with non-undoable readers */
	void undo(uint n) override { read_impl::undo(n); }

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
		uint n = min<uint>(read_ushort(), read_impl::available());
		str.resize(n);
		read_impl::read((void*)str.data(), sizeof(Char) * n);
		return *this;
	}
	/** @brief Reads a length specified string to the dst buffer in the form of [uint16 len][data] and returns actual length */
	template<class Char> uint read_nstr(Char* dst, uint maxLen) {
		uint n = min3<uint>(read_ushort(), read_impl::available(), maxLen);
		read_impl::read((void*)dst, sizeof(Char) * n);
		return n;
	}
	/** @brief Peeks a length specified string to the std::string in the form of [uint16 len][data] */
	template<class Char> binary_reader& peek(std::basic_string<Char>& str) {
		uint n = min<uint>(read_ushort(), read_impl::available());
		str.resize(n);
		read_impl::peek((void*)str.data(), sizeof(Char) * n);
		undo(2); // undo read_ushort
		return *this;
	}
	/** @brief Peeks a length specified string to the dst buffer in the form of [uint16 len][data] and returns actual length */
	template<class Char> uint peek_nstr(Char* dst, uint maxLen) {
		uint n = min3<uint>(read_ushort(), read_impl::available(), maxLen);
		n = read_impl::peek((void*)dst, sizeof(Char) * n) / sizeof(Char);
		undo(2); // undo read_ushort
		return n;
	}

	std::string  read_string()  { std::string  s; read(s); return s; } /** @brief Reads a length specified [uint16 len][data] string */
	std::wstring read_wstring() { std::wstring s; read(s); return s; } /** @brief Reads a length specified [uint16 len][data] wstring */
	std::string  peek_string()  { std::string  s; peek(s); return s; } /** @brief Peeks a length specified [uint16 len][data] string */
	std::wstring peek_wstring() { std::wstring s; peek(s); return s; } /** @brief Peeks a length specified [uint16 len][data] wstring */
	strview      peek_strview() { strview s; peek(s); return s; }      /** @brief Peeks a length specified [uint16 len][data] string view */

	/**
	 * @brief Reads a vector as [uint16 len] [ len * sizeof(T) ]
	 * @note  May require operator>> if T is not a POD type: 
	 *            binary_reader& operator>>(binary_reader& rb, T& out);
	 */
	template<class T, class U> uint read(std::vector<T, U>& vec) {
		int count = read_ushort();
		vec.resize(count);
		T* data = vec.data();
		if (std::is_pod<T>::value) 
			return read(data, sizeof(T) * count) / sizeof(T);
		else for (int i = 0; i < count; ++i) 
				*this >> data[i];
		return count; // TODO: fix this bug
	}
};
// explicit operator>> overloads
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, strview& v)      { r.peek(v); return r; }
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, std::string& v)  { r.read(v); return r; }
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, std::wstring& v) { r.read(v); return r; }
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, bool& v)   { r.read(v); return r; }
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, char& v)   { r.read(v); return r; }
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, byte& v)   { r.read(v); return r; }
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, short& v)  { r.read(v); return r; }
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, ushort& v) { r.read(v); return r; }
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, int& v)    { r.read(v); return r; }
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, uint& v)   { r.read(v); return r; }
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, int64& v)  { r.read(v); return r; }
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, uint64& v) { r.read(v); return r; }
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, float& v)  { r.read(v); return r; }
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, double& v) { r.read(v); return r; }
template<class C, class T, class U> binary_reader<C>& operator>>(binary_reader<C>& r, std::vector<T, U>& v) { r.read(v); return r; }
template<class C> binary_reader<C>& operator>>(binary_reader<C>& r, binary_reader<C>& m(binary_reader<C>&)) { return m(r); }





struct view_read_base // most generic data reading interface
{
	typedef byte* ptr_t;
	uint Pos;   // current streampos
	uint Rem;   // remaining bytes
	void*Ptr;   // data reference
	uint Max;   // max buffer capacity

	view_read_base(void* ptr, uint max)           : Pos(0),Rem(0),  Ptr(ptr),Max(max) {}
	view_read_base(void* ptr, uint rem, uint max) : Pos(0),Rem(rem),Ptr(ptr),Max(max) {}

	uint max()       const { return Max; }
	uint pos()       const { return Pos; }
	uint size()      const { return Pos + Rem; }
	uint available() const { return Rem; }
	void flush() { Pos = Rem = 0; } /* flush clears base buffer state */

	uint read(void* dst, uint cnt) {
		uint n = peek(dst,cnt); 
		Pos += n, Rem -= n; 
		return n; 
	}
	template<class T> uint read(T& dst)  {
		uint n = peek(dst);     
		Pos += n, Rem -= n; 
		return n; 
	}
	uint peek(void* dst, uint cnt) {
		uint n = min<uint>(cnt, Rem);
		memcpy(dst, ptr_t(Ptr) + Pos, n);
		return n;
	}
	template<class T> uint peek(T& dst) {
		uint n = min<uint>(sizeof(T), Rem);
		if (n < sizeof(T)) return 0; // can't peek
		dst = *(T*)(ptr_t(Ptr) + Pos);
		return n;
	}
	void skip(uint n) {
		uint nskip = min<uint>(n, Rem); // max skippable: Rem
		Pos += nskip, Rem -= nskip;
	}
	void undo(uint n) {
		uint nundo = min<uint>(n, Pos); // max undoable:  Pos
		Pos -= nundo, Rem += nundo;
	}
};

/**
 * A static array read buffer. Useless without a backing read source - use composite_read (!)
 * A small default array size is provided as 512 bytes.
 */
template<uint MAX = 512> struct array_read : public view_read_base
{
	byte Buf[MAX];

	array_read() : view_read_base(Buf, MAX) {}
	NOCOPY_NOMOVE(array_read)
	
	template<class storage> void fill(storage& st) {
		Pos = 0, Rem = st.read(Ptr, MAX);
	}
};


/**
 * A static view read buffer. Should be initialized with data to be read.
 * The read data buffer is allocated somewhere else and this class merely holds
 * a pointer to the data.
 * @note This class can't be used with combined_read<buffer,storage>
 * @note Only use this to parse data from an existing data source buffer:
 *       view_reader vr(data, length);
 *       int x, y;
 *       vr >> x >> y;
 *       int z = vr.read_int();
 */
struct view_read : public view_read_base
{
	/** Initializes view_read for reading data from the given buffer */
	explicit view_read(const void* buf, uint len)  : view_read_base((void*)buf, len, len) {}
	/** Initializes view_read for reading data from the specified vector */
	explicit view_read(const std::vector<byte>& v) : view_read(v.data(), (uint)v.size()) {}
	NOCOPY_MOVE(view_read)
	
	template<class storage> void fill(storage& st) {
		Pos = 0, Rem = st.read(Ptr, Max);
	}
};


/**
 * A dynamic array read buffer. Useless without a backing read source - use composite_read (!)
 * This buffer will dynamically set its size to match backing storage, allowing 
 * large dynamic buffering. Default Max size of dynamic buffer is INT_MAX (2GB), but can be overridden.
 */
struct buffer_read : public view_read_base
{
	buffer_read()         : view_read_base(nullptr, 0x7fffffff) {}
	buffer_read(uint max) : view_read_base(nullptr, max) {}
	~buffer_read() { if (Ptr) free(Ptr); }
	NOCOPY_MOVE(buffer_read)
	
	template<class storage> void fill(storage& st) {
		Pos = 0, Rem = min<uint>(st.size(), Max);
		Ptr = realloc(Ptr, Rem);  // allocate 
		Rem = st.read(Ptr, Rem);
	}
};


/**
 * A file reader. Uses FILE* C API.
 */
struct file_read : public view_read_base
{
	typedef FILE* ptr_t;

	file_read(FILE* file) : view_read_base(file,0) {Rem=Max=filesize();}
	file_read(const char* path,        const char* mode = "wb") : file_read(fopen(path, mode)) {}
	file_read(const std::string& path, const char* mode = "wb") : file_read(fopen(path.c_str(), mode)) {}
	uint filesize() { struct stat s; fstat(fileno(ptr_t(Ptr)), &s); return s.st_size; }
	~file_read() { if (Ptr) fclose(ptr_t(Ptr)); }
	NOCOPY_MOVE(file_read)
	
	void flush() { fflush(ptr_t(Ptr)); } /* flush clears any FILE* input buffering */

	template<class T> uint read(T& dst) { return read(&dst, sizeof(T)); }
	template<class T> uint peek(T& dst) { return peek(&dst, sizeof(T)); }
	uint read(void* dst, uint cnt) {
		uint n = min<uint>(cnt, Rem);
		if (!n) return 0;
		fread(dst, n, 1, ptr_t(Ptr));
		return n;
	}
	uint peek(void* dst, uint cnt) {
		uint n = read(dst, cnt); undo(n); return n;
	}
	void skip(uint n) {
		view_read_base::skip(n);          // all magic in superclass
		fseek(ptr_t(Ptr), Pos, SEEK_SET); // we just set the new streampos
	}
	void undo(uint n) {
		view_read_base::undo(n);          // all magic in superclass
		fseek(ptr_t(Ptr), Pos, SEEK_SET); // we just set the new streampos
	}
};


/**
 * Reads binary data from a socket, socket is kept as a pointer.
 */
struct socket_read
{
	uint Pos;            // total bytes read from socket
	rpp::socket* Socket; // as pointer to allow Move

	explicit socket_read(rpp::socket& s) : Pos(0), Socket(&s) {}
	NOCOPY_MOVE(socket_read)
	
	uint max()       const { return 0xffffffff; /* network is unlimited */ }
	uint pos()       const { return Pos; } /* network stream total bytes read */
	uint size()      const { return Pos; } /* network stream total bytes read */
	uint available() const { return Socket->available(); }
	void flush()           { if (available()) Socket->flush(); }

	template<class T> uint read(T& dst) { return available() >= sizeof(T) ? read(&dst, sizeof(T)) : 0; }
	template<class T> uint peek(T& dst) { return peek(&dst, sizeof(T)); }
	uint read(void* dst, uint cnt) {
		int n = Socket->recv(dst, cnt);
		if (n <= 0) return 0;
		Pos += n;
		return n;
	}
	uint peek(void* dst, uint cnt) {
		return rpp::max(0, Socket->peek(dst, cnt));
	}
	void skip(uint n) {
		Socket->skip(n);
		Pos += n;
	}
	void undo(uint n) { } /* socket can't undo! */
};


/**
 * A composite read utilizes first type as a read buffer to reduce the number of I/O calls
 * and secondary type as data source for reads (file/socket/etc)
 */
template<class buffer, class storage> struct composite_read
	: private storage, private buffer
{
	using storage::storage; // inherit Storage class constructors

	uint max() const { return storage::max(); }
	uint pos() const { 
		uint i = storage::pos(); // some storage classes may report 0 pos
		return i ? i - buffer::available() : 0;
	}
	uint size()      const { return storage::size(); }
	uint available() const { return buffer::available() + storage::available();  }
	void flush()           { buffer::flush(), storage::flush(); } /* flushes both buffered data and any storage buffering */

	uint read(void* dst, uint cnt) {
		uint n = buffer::available();
		if (n >= cnt) return buffer::read(dst, cnt); // best case, all from buffer
		return _read(&dst, cnt, n);                  // fallback partial read
	}
	template<class T> uint read(T& dst) {
		uint n = buffer::available();
		if (n >= sizeof(T)) return buffer::read(dst);  // best case, all from buffer
		if (n+storage::available() < sizeof(T)) return 0; // can't read whole T
		return _read(&dst, sizeof(T), n);                 // fallback partial read
	}
	uint _read(void* dst, uint cnt, uint bufn) {
		uint rem = cnt - buffer::read(dst, bufn); // read some from buffer; won't read all(!)
		if (rem >= buffer::max())
			return bufn + storage::read((char*)dst+bufn, rem); // straight from storage
		buffer::fill((storage&)*this);
		return bufn + buffer::read((char*)dst+bufn, rem); // refill and read from buf
	}
	uint peek(void* dst, uint cnt) {
		if (!buffer::available())
			buffer::fill((storage&)*this); // fill before peek if possible
		return buffer::peek(dst, cnt);     // peek what we can, might not be enough
	}
	template<class T> uint peek(T& dst) {
		if (!buffer::available())
			buffer::fill((storage&)*this); // fill before peek if possible
		return buffer::peek(dst);          // peek what we can, might not be enough
	}
	void skip(uint n) {
		uint bufskip = min<uint>(n, buffer::available());
		buffer::skip(bufskip);
		if (bufskip < n) storage::skip(n - bufskip); // skip remaining from storage
	}
	void undo(uint n) {
		uint bufundo = min<uint>(n, buffer::pos());
		buffer::undo(bufundo);
		if (bufundo < n) storage::undo(n - bufundo); // undo remaining from storage
	}
};


//////////////////// some predefined compositions ////////////////////


/** @brief Reads data from a fixed sized array. Default size 512 bytes.  */
template<uint SIZE = 512> using array_reader = binary_reader<array_read<SIZE>>;
/** @brief Reads data from an array view. Array size depends on its initialized view size. */
typedef binary_reader<view_read>   view_reader;
/** @brief Reads data from a dynamic buffer, possibly after a FILE read has been buffered. */
typedef binary_reader<buffer_read> buffer_reader;
/** @brief Reads data from file using C FILE* API. */
typedef binary_reader<file_read>   file_reader;
/** @brief Reads data directly from an rpp::socket */
typedef binary_reader<socket_read> socket_reader;
/**
 * @brief A stream reader combines a primary read buffer and a source storage reader
 *        All data is buffered by the buffer class, some options for buffer:
 *		      array_read<512>  - reads into a fixed sized array buffer; flush() to discard buffered data
 *		      view_read        - reads into an array view of some buffer; flush() to discard buffered data
 *		      buffer_read      - reads all data into a buffer;
 *		  Storage class is only used for data flushing, some options:
 *		      file_read        - reads from a C FILE
 *		      socket_read      - reads from an rpp::socket
 */
template<class buffer, class storage> using stream_reader = binary_reader<composite_read<buffer, storage>>;


template<uint SIZE = 512>
using socket_arraystream_reader  = stream_reader<array_read<SIZE>, socket_read>;
using socket_bufferstream_reader = stream_reader<buffer_read, socket_read>;
template<uint SIZE = 512>
using file_arraystream_reader  = stream_reader<array_read<SIZE>, file_read>;
using file_bufferstream_reader = stream_reader<buffer_read, file_read>;

} // namespace rpp

#endif // RPP_BINARY_READER_H
