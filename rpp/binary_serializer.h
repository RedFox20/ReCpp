#pragma once
#ifndef RPP_BINARY_SERIALIZER_H
#define RPP_BINARY_SERIALIZER_H
#include "binary_readwrite.h"

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

		NOINLINE socket_writer& write(socket_writer& w) const noexcept;
		NOINLINE socket_reader& read(socket_reader& r) noexcept;

		/** @brief Returns the serialization layout size */
		NOINLINE int layout_size() const noexcept;
	};

	inline socket_writer& operator<<(socket_writer& w, const binary_serializer& s) 
	{
		return s.write(w); 
	}
	inline socket_reader& operator>>(socket_reader& r, binary_serializer& s) 
	{
		return s.read(r); 
	}

} // namespace rpp

#endif // RPP_BINARY_SERIALIZER_H
