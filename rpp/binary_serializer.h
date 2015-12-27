#pragma once
#include "binary_reader.h"
#include "binary_writer.h"
#include <assert.h>

//// @note Some functions get inlined too aggressively, leading to some serious code bloat
////       Need to hint the compiler to take it easy ^_^'
#ifndef NOINLINE
#ifdef _MSC_VER
#  define NOINLINE __declspec(noinline)
#else
#  define NOINLINE __attribute__((noinline))
#endif
#endif

namespace rpp
{
	template<int SIZE> constexpr uint64 _layoutv(const char (&s)[SIZE], uint64 i, char v) {
		return i < SIZE - 1 ? (s[i] == v ? i : _layoutv(s, i + 1, v)) : 0ull;
	}
	template<int SIZE> constexpr uint64 _layout(const char (&s)[SIZE], uint64 i = 0) {
		return i < SIZE - 1 ? (_layout(s, i + 1) * 10ull + _layoutv("?bwdqs", 0, s[i])) : 0ull;
	}
	// Provide an autoserialize string that describes the data layout
	// "bwdqs" 
	// b - byte   (8-bit  - bool, char, uint8)
	// w - word   (16-bit - short, uint16)
	// d - dword  (32-bit - int, uint32)
	// q - qword  (64-bit - int64, uint64)
	// s - string (std::string)
	#define BINARY_SERIALIZE(Class, layoutString) \
		static const rpp::uint64 Layout = rpp::_layout(layoutString); \
		template<class C> inline Class(rpp::binary_reader<C>& r){layout=Layout; this->read(r);} \
		inline Class(const std::vector<uint8_t>& v){layout=Layout; rpp::view_reader r(v); this->read(r);}

	struct binary_serializer
	{
		uint64 layout;
		// numbytes during read/write, initialized during read, uninitialized by default
		// used to skip messages and to validate the message
		int length;

		binary_serializer() {}
		binary_serializer(uint64 layout) : layout(layout), length(0) {}

		template<class C> NOINLINE binary_writer<C>& write(binary_writer<C>& w) const noexcept
		{
			// we write[int32 length][layout data...]
			int size = layout_size() + sizeof(int);
			w.write_int(size);
			auto p = (char*)this + sizeof(*this);

			for (auto v = layout; v > 0; v /= 10) switch (v % 10) {
				case 1: w << *(char*)p;        p += 1; continue;
				case 2: w << *(short*)p;       p += 2; continue;
				case 3: w << *(int*)p;         p += 4; continue;
				case 4: w << *(__int64*)p;     p += 8; continue;
				case 5: w << *(std::string*)p; p += sizeof(std::string); continue;
			}
			return w;
		}
		template<class C> NOINLINE binary_reader<C>& read(binary_reader<C>& r) noexcept
		{
			// we don't read layouts sent by remote end, but we read length for validation
			length = r.read_int();
			int size = sizeof(length);
			auto p = (char*)this + sizeof(*this);

			for (auto v = layout; v > 0; v /= 10) switch (v % 10) {
				case 1: r >> *(char*)p;    p += 1; size += 1; continue;
				case 2: r >> *(short*)p;   p += 2; size += 2; continue;
				case 3: r >> *(int*)p;     p += 4; size += 4; continue;
				case 4: r >> *(__int64*)p; p += 8; size += 8; continue;
				case 5: {
					r >> *(std::string*)p;
					size += 2 + (int)((std::string*)p)->size();
					p += sizeof(std::string);
					continue;
				}
				default:
					assert(!"AutoSerialize: Invalid layout specifier.");
					v = 0; // stop deserialization
			}
			if (length != size) {
				fprintf(stderr, "AutoSerialize: Layout length doesn't match binary data!");
				// TODO: this requires some graceful handling
				length = -length; // flip length to show that it's invalid...
			}
			return r;
		}

		/** @brief Returns the serialization layout size */
		int layout_size() const noexcept;
	};
	template<class C> inline binary_writer<C>& operator<<(binary_writer<C>& w, const binary_serializer& s) 
	{ 
		return s.write(w); 
	}
	template<class C> inline binary_reader<C>& operator>>(binary_reader<C>& r, binary_serializer& s) 
	{
		return s.read(r); 
	}

} // namespace rpp
