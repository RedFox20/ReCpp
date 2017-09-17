#include "binary_serializer.h"
#include <assert.h>
#include <stdio.h>

namespace rpp
{
	NOINLINE binary_stream& binary_serializer::write(binary_stream & w) const noexcept
	{
		// we write[int32 length][layout data...]
		const int size = layout_size();
		w.write_int(size); // write length

		static const int sizeOf = sizeof(binary_serializer);
		auto p = (char*)this + sizeOf;

		for (auto v = layout; v > 0; v /= 10) switch (v % 10) {
		case 1: w << *(char*)p;        p += 1; continue;
		case 2: w << *(short*)p;       p += 2; continue;
		case 3: w << *(int*)p;         p += 4; continue;
		case 4: w << *(int64*)p;       p += 8; continue;
		case 5: w << *(std::string*)p; p += sizeof(std::string); continue;
		}
		return w;
	}
	NOINLINE binary_stream& binary_serializer::read(binary_stream & r) noexcept
	{
		// we don't read layouts sent by remote end, but we read length for validation
		length = r.read_int();
		int size = sizeof(length);
		auto p = (char*)this + sizeof(*this);

		for (auto v = layout; v > 0; v /= 10) switch (v % 10) {
		case 1: r >> *(char*)p;    p += 1; size += 1; continue;
		case 2: r >> *(short*)p;   p += 2; size += 2; continue;
		case 3: r >> *(int*)p;     p += 4; size += 4; continue;
		case 4: r >> *(int64*)p;   p += 8; size += 8; continue;
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
	NOINLINE int binary_serializer::layout_size() const noexcept
	{
		int size = sizeof(length);
		static const int sizeOf = sizeof(binary_serializer);
		auto p = (char*)this + sizeOf;

		for (auto v = layout; v > 0; v /= 10) switch (v % 10) {
			case 1: p += 1; size += 1; continue;
			case 2: p += 2; size += 2; continue;
			case 3: p += 4; size += 4; continue;
			case 4: p += 8; size += 8; continue;
			case 5: {
				const std::string& str = *(std::string*)p; // dangerous stuff...
				size += 2 + (int)str.size();
				p += sizeof(std::string);
				continue;
			}
			default:
				assert(!"AutoSerialize: Invalid layout specifier.");
				v = 0; // stop deserialization
		}
		return size;
	}
} // namespace rpp